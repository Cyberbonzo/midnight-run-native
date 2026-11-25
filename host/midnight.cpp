#include <SDL3/SDL.h>
#include "steering.h"
extern "C" {
#include "../cpu/ppc403.h"
#include "../board/zr107.h"
#include "../video/texel_bus.h"
}
#include "../sound/sound_cpu.h"
#include "../video/sharc/dsp.h"
#include "../board/zr_raster.h"
#include <algorithm>
#include <array>
#include <vector>
#include <fstream>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace fs=std::filesystem;
using Bytes=std::vector<uint8_t>;
static Bytes read(const fs::path &path,size_t size){
    std::ifstream f(path,std::ios::binary);
    if(!f||fs::file_size(path)!=size) throw std::runtime_error("Invalid asset: "+path.string());
    Bytes b(size);if(!f.read(reinterpret_cast<char*>(b.data()),size))throw std::runtime_error("Read failed: "+path.string());return b;
}
struct Board {
    Bytes program,tiles,sound,samples,nvram;
    std::array<Bytes,4> lanes;
    std::unique_ptr<ZRBoard,decltype(&zr_destroy)> zr{nullptr,zr_destroy};
    std::unique_ptr<ZRRaster,decltype(&zr_raster_destroy)> raster{nullptr,zr_raster_destroy};
    std::unique_ptr<ZRDSP,decltype(&zr_dsp_destroy)> dsp{nullptr,zr_dsp_destroy};
    Ppc403 ppc{};
    ZRTexelBus tex{};
    bool sound_reset=true;
    unsigned audio_remainder=0;
    Board(const fs::path &dir,const fs::path &nv):program(read(dir/"program.bin",0x200000)),
        tiles(read(dir/"tiles.bin",0x100000)),sound(read(dir/"sound.bin",0x20000)),samples(read(dir/"samples.bin",0x600000)),
        nvram(read(nv,128)){
        const std::array<const char*,4> names={"477a13.9h","477a14.7h","477a15.5h","477a16.2h"};
        const uint8_t *ptr[4];
        for(int i=0;i<4;i++){lanes[i]=read(dir/names[i],0x200000);ptr[i]=lanes[i].data();}
        zr.reset(zr_create(program.data(),program.size()));
        if(!zr||!zr_set_tiles(zr.get(),tiles.data(),tiles.size())||
           !zr_set_soundrom(zr.get(),sound.data(),sound.size())||
           !zr_set_samples(zr.get(),samples.data(),samples.size())||
           !zr_eeprom_load(zr.get(),nvram.data())||!zr_texel_init(&tex,ptr,0x200000))
            throw std::runtime_error("Could not initialize ZR107");
        zr_set_video_io(zr.get(),&tex,zr_texel_read,zr_texel_write);
        raster.reset(zr_raster_create(zr.get(),&tex,683));
        dsp.reset(zr_dsp_create(zr.get()));
        if(!raster||!dsp)throw std::runtime_error("Could not initialize graphics");
        zr_raster_enable_midpoints(raster.get(),true);
        ppc.pc=0xfffffffc;ppc.board=zr.get();ppc.read=zr_read;ppc.write=zr_write;
    }
    void frame(int16_t *pcm){
        zr_vblank(zr.get());
        unsigned units=0, audio_frames=0;
        // Interleave communication between the main program, audio program and sample timers.
        constexpr unsigned ticks=508626;
        while(units<ticks){
            unsigned n=std::min(ticks-units,1024u);
            ppc403_irq(&ppc,zr_ppc_irq(zr.get()));int code=ppc403_run(&ppc,n);
            if(code){char msg[140];std::snprintf(msg,sizeof(msg),"PPC %08x: status %d, bus %08x/%u, opcode %08x",ppc.pc,code,ppc.fault_addr,ppc.fault_size,ppc.fault_word);throw std::runtime_error(msg);}
            if(zr_dsp_run(dsp.get(),n*9/8)<0){char msg[360];std::snprintf(msg,sizeof(msg),"SHARC %06x, PPC %08x: %s",zr_dsp_pc(dsp.get()),ppc.pc,zr_dsp_error(dsp.get()));throw std::runtime_error(msg);}
            bool reset=zr_sound_in_reset(zr.get());
            if(!reset){
                if(sound_reset)zrsound_reset(zr.get());
                audio_remainder+=n;
                unsigned quota=audio_remainder/32;audio_remainder%=32;
                if(!zrsound_run(zr.get(),quota))throw std::runtime_error("Audio program stopped");
            }
            sound_reset=reset;
            units+=n;
            unsigned target=(uint64_t(units)*800)/ticks;
            if(target>audio_frames)zr_audio_render(zr.get(),pcm+2*audio_frames,target-audio_frames);
            audio_frames=target;
        }
    }
};
static constexpr int w=683,h=384;
struct Display {
    CabinetSteering steering;
    SDL_Window *window=nullptr;
    SDL_Renderer *renderer=nullptr;
    SDL_Texture *texture=nullptr;
    SDL_Gamepad *pad=nullptr;
    SDL_AudioStream *audio=nullptr;
    ~Display(){if(audio)SDL_DestroyAudioStream(audio);if(pad)SDL_CloseGamepad(pad);if(texture)SDL_DestroyTexture(texture);if(renderer)SDL_DestroyRenderer(renderer);if(window)SDL_DestroyWindow(window);SDL_Quit();}
    void open(){
        if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMEPAD|SDL_INIT_AUDIO))throw std::runtime_error(SDL_GetError());
        if(!SDL_CreateWindowAndRenderer("Midnight Run: Road Fighter 2",1920,1080,SDL_WINDOW_FULLSCREEN,&window,&renderer))throw std::runtime_error(SDL_GetError());
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,w,h);
        if(!texture)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);
        auto ids=SDL_GetGamepads(nullptr);if(ids){if(*ids)pad=SDL_OpenGamepad(*ids);SDL_free(ids);}
        SDL_AudioSpec spec{SDL_AUDIO_S16,2,48000};
        audio=SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,&spec,nullptr,nullptr);
        if(audio)SDL_ResumeAudioStreamDevice(audio);
    }
    bool input(ZRBoard *board){
        SDL_Event e;while(SDL_PollEvent(&e)){
            if(e.type==SDL_EVENT_QUIT)return false;
            if(e.type==SDL_EVENT_GAMEPAD_ADDED&&!pad)pad=SDL_OpenGamepad(e.gdevice.which);
            if(e.type==SDL_EVENT_GAMEPAD_REMOVED&&pad&&e.gdevice.which==SDL_GetGamepadID(pad)){SDL_CloseGamepad(pad);pad=nullptr;}
        }
        auto k=SDL_GetKeyboardState(nullptr);
        if(k[SDL_SCANCODE_ESCAPE])return false;
        auto axis=[&](SDL_GamepadAxis a){return pad?(int)SDL_GetGamepadAxis(pad,a):0;};
        auto button=[&](SDL_GamepadButton b){return pad&&SDL_GetGamepadButton(pad,b);};
        int wheel=steering.update(axis(SDL_GAMEPAD_AXIS_LEFTX));
        if(k[SDL_SCANCODE_LEFT])wheel=0;
        else if(k[SDL_SCANCODE_RIGHT])wheel=255;
        int gas=k[SDL_SCANCODE_UP]?128:std::max(0,axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))*128/32767;
        int brake=k[SDL_SCANCODE_DOWN]?128:std::max(0,axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER))*128/32767;
        uint8_t buttons=0;
        if(button(SDL_GAMEPAD_BUTTON_SOUTH)||button(SDL_GAMEPAD_BUTTON_START)||k[SDL_SCANCODE_RETURN])buttons|=0x80;
        if(button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)||k[SDL_SCANCODE_E])buttons|=0x40;
        if(button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)||k[SDL_SCANCODE_Q])buttons|=0x20;
        if(button(SDL_GAMEPAD_BUTTON_WEST)||k[SDL_SCANCODE_X])buttons|=0x10;
        zr_set_inputs(board,wheel,gas,brake,buttons,button(SDL_GAMEPAD_BUTTON_BACK)||k[SDL_SCANCODE_5]);
        return true;
    }
    void present(const uint32_t* pix,const int16_t* pcm){
        SDL_UpdateTexture(texture,nullptr,pix,w*4);int pw,ph;SDL_GetCurrentRenderOutputSize(renderer,&pw,&ph);
        float scale=std::min(pw/16.0f,ph/9.0f);
        SDL_FRect area={(pw-16*scale)/2,(ph-9*scale)/2,16*scale,9*scale};
        SDL_RenderClear(renderer);SDL_RenderTexture(renderer,texture,nullptr,&area);SDL_RenderPresent(renderer);
        if(audio&&SDL_GetAudioStreamQueued(audio)<48000*4/8)SDL_PutAudioStreamData(audio,pcm,800*4);
    }
};
int main(int argc,char** argv){
    try{
        if(argc<2){std::fprintf(stderr,"Usage: midnight assets [--headless frames [frame.ppm]]\n");return 2;}
        fs::path dir=argv[1];bool hidden=argc>=4 && std::string(argv[2])=="--headless";
        fs::path data=dir/"default.eeprom",save;
        if(!hidden){
            auto pref=SDL_GetPrefPath("Konami-Ports","MidnightRun");
            if(!pref)throw std::runtime_error(SDL_GetError());
            save=fs::path(pref)/"cabinet.eeprom";SDL_free(pref);
            if(fs::exists(save))data=save;
        }
        Board b(dir,data);Display display;if(!hidden)display.open();
        std::vector<uint32_t> pixels(w*h), previous_scene(w*h), current_scene(w*h);int16_t pcm[1600]{};
        auto deadline=std::chrono::steady_clock::now();unsigned frame=0;
        unsigned limit=hidden?std::stoul(argv[3]):0;
        bool scripted=hidden && std::getenv("MIDNIGHT_TEST_DRIVE");
        bool telemetry=std::getenv("MIDNIGHT_PERF_LOG")!=nullptr;
        auto persist=[&](){
            if(hidden)return;
            std::array<uint8_t,128> current{};zr_eeprom_dump(b.zr.get(),current.data());
            if(std::equal(current.begin(),current.end(),b.nvram.begin()) && fs::exists(save))return;
            fs::path tmp=save;tmp+=".tmp";
            std::ofstream file(tmp,std::ios::binary|std::ios::trunc);
            file.write((const char*)current.data(),current.size());file.close();
            if(!file)throw std::runtime_error("Error writing cabinet settings");
            fs::rename(tmp,save);
            b.nvram.assign(current.begin(),current.end());
        };
        auto measured=std::chrono::steady_clock::now();
        uint64_t last_swaps=0, presented_swap=0, midpoints=0;
        bool have_scene=false, pending_midpoint=false;
        bool benchmark=hidden && std::getenv("MIDNIGHT_TEST_RENDER");
        while(!hidden||frame<limit){
            if(!hidden&&!display.input(b.zr.get()))break;
            if(scripted){
                bool coin=frame>=1100 && frame<1500 && frame%30<5;
                bool start=frame>=1500 && frame<2600 && frame%60<6;
                int wheel=frame<2700?128:(frame%360<180?160:96);
                zr_set_inputs(b.zr.get(),wheel,frame>=2100?128:0,0,start?0x80:0,coin);
            }
            b.frame(pcm);
            if(!hidden || benchmark || frame+1==limit){
                const auto swaps=zr_raster_stats(b.raster.get()).swaps;
                auto background=[&](std::vector<uint32_t>& dst){
                    zr_clear_frame(b.zr.get(),dst.data(),w,w,h);
                    zr_draw_tiles(b.zr.get(),1,dst.data(),w,w,h);
                };
                if(swaps!=presented_swap){
                    previous_scene.swap(current_scene);
                    background(current_scene);
                    zr_draw_geometry(b.raster.get(),current_scene.data(),w,w,h);
                    pixels=have_scene?previous_scene:current_scene;
                    pending_midpoint=have_scene;
                    have_scene=true;
                }else if(pending_midpoint){
                    background(pixels);
                    if(zr_draw_geometry_midpoint(b.raster.get(),pixels.data(),w,w,h))++midpoints;
                    else pixels=current_scene;
                    pending_midpoint=false;
                }else{
                    background(pixels);
                    zr_draw_geometry(b.raster.get(),pixels.data(),w,w,h);
                }
                presented_swap=swaps;
                zr_draw_tiles(b.zr.get(),0,pixels.data(),w,w,h);
            }
            if(!hidden){
                display.present(pixels.data(),pcm);
                deadline+=std::chrono::nanoseconds(16666667);
                std::this_thread::sleep_until(deadline);
                if(std::chrono::steady_clock::now()-deadline>std::chrono::milliseconds(150))deadline=std::chrono::steady_clock::now();
            }
            ++frame;
            if(frame%60==0){
                if(hidden||telemetry){
                    auto now=std::chrono::steady_clock::now();
                    auto st=zr_raster_stats(b.raster.get());
                    std::printf("frame %u main %08x sound %06x dsp %06x swaps %llu triangles %llu midpoints %llu sim-fps %.1f\n",frame,b.ppc.pc,zrsound_pc(),zr_dsp_pc(b.dsp.get()),(unsigned long long)(st.swaps-last_swaps),(unsigned long long)st.triangles,(unsigned long long)midpoints,60.0/std::chrono::duration<double>(now-measured).count());
                    std::fflush(stdout);measured=now;last_swaps=st.swaps;
                }
                if(!hidden)persist();
            }
        }
        persist();
        if(hidden&&argc>4){
            std::ofstream f(argv[4],std::ios::binary);f<<"P6\n"<<w<<" "<<h<<"\n255\n";
            for(auto p:pixels){char rgb[3]={(char)(p>>16),(char)(p>>8),(char)p};f.write(rgb,3);}
        }
        return 0;
    }catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}

