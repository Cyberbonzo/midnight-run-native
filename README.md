# Midnight Run — Road Fighter 2

Recompilação nativa para Linux x86-64 da edição europeia EAA 1.11 do arcade de 1995. O executável integra o programa PowerPC, o áudio 68000 e o firmware gráfico SHARC recompilados a partir de uma cópia do próprio jogador. A apresentação e a entrada usam SDL3.

## ROG Ally

O jogo amplia horizontalmente a projeção da pista e preserva as proporções do carro, do painel e do retrovisor. O raster interno mede 683 × 384 e a janela ocupa 1920 × 1080 no painel do Ally. A lógica e a geometria originais avançam a 30 Hz durante a corrida. A apresentação tem alvo de 60 FPS e reconstrói um quadro intermediário pela posição dos vértices quando consegue relacionar as duas cenas. Em cortes de câmera ou cenas estáticas, a apresentação pode repetir uma imagem. Os menus usam o layout do arcade.

O velocímetro usa a opção original **Km/h** no menu de operador da edição europeia. As configurações de gabinete ficam no arquivo de 128 bytes `cabinet.eeprom` do diretório de preferências de usuário SDL, salvo automaticamente.

| Função | Controle |
| --- | --- |
| Direção | Analógico esquerdo |
| Acelerar ou confirmar o carro | RT |
| Frear | LT |
| Inserir ficha | View |
| Iniciar ou mudar a câmera | A ou Menu |
| Alternar automático e manual | X |
| Subir ou reduzir marcha | RB e LB |

A direção tem zona morta central de 5%, curva progressiva e retorno rápido ao centro. Metade do curso do analógico corresponde a 25% de volante. As pontas alcançam o esterço completo. Os gatilhos são analógicos.

## Compilar a partir do seu arquivo

Requer Python 3, GCC e G++ com C++20, GNU Make e SDL3 para Linux. O projeto usa os cabeçalhos SDL3 do sistema ou da pasta local `include/SDL3` e vincula o executável a `libSDL3.so.0`. A configuração verifica os 14 chips pelos hashes antes de gerar arquivos privados. Execute na raiz do projeto.

```sh
python3 host/configure.py "/caminho/para/midnrun.zip"
bash host/build-linux.sh
./midnight private/assets
```

A primeira inicialização pode exibir o autoteste do gabinete. No teclado, 5 insere ficha, Enter inicia e as setas controlam o volante e os pedais. Para um ensaio isolado de corrida com fichas, seleção de carro, aceleração e curvas, execute o comando abaixo. Essa sessão usa sua própria memória temporária de gabinete.

```sh
MIDNIGHT_TEST_DRIVE=1 MIDNIGHT_TEST_RENDER=1 ./midnight private/assets --headless 3300 corrida.ppm
```

## Verificação no Bazzite

A instalação foi aberta pela biblioteca Steam no ROG Ally. A janela reportou 1920 × 1080. Uma captura da corrida mostrou 201 km/h em terceira posição. Foram conferidos os oito testes de diagnóstico gráfico e os extremos, o centro, a progressão e a inversão da direção. A meta de 60 FPS depende da carga da cena e do perfil de energia.

## Créditos

O tradutor 68000 e seu cabeçalho de execução vêm do trabalho de Tobic-99, sob GPLv3. Os arquivos de SHARC preservam os avisos BSD originais. Os geradores locais e os dados fornecidos pelo jogador ficam em diretórios separados dos fontes versionados. A referência da placa e os testes de renderização estão documentados no projeto. [demonfront-vita](https://github.com/Tobic-99/demonfront-vita) · [SHARC](video/sharc/README.md) · [ZR107](board/SOURCES.md) · [Licença](LICENSE)
