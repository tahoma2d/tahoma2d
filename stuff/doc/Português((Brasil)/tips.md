# Bem-vindo ao Tahoma2D 1.6!

A Versão 1.5 traz um grande número de novos recursos, aprimoramentos e ainda mais correções que estão listadas [aqui](https://github.com/tahoma2d/tahoma2d/releases/tag/v1.5).

### Compatibility Changes

- Column Clipping Mask placement and stacking order has changed. It should now be placed above the column to be clipped and stacks bottom->top/left->right
- Palettes with Raster based custom styles (`Style Editor` -> `Vector` tab -> `Custom styles`) will be auto upgraded to new format which will prevent use in prior versions of T2D if the paleette is saved in the latest T2D version.
- Palettes with newly added styles `Marker` and `Soft Round` (`Style Editor` -> `Vector` tab -> `Generated`) will prevent use in prior versions of T2D

### Usuários de Windows

Se estiver tendo problemas com canetas digitais nesta versão, especialmente se estiver usando um notebook-tablet/Tablet Surface tente uma ou as duas sugestões:
- Habilite `Preferências` -> `Configurações de Toque/Tablet` -> `Usar suporte nativo do Qt para Windows Ink*` e reinicie o Tahoma2D.
- Mantenha a opção acima ativa e instale os drivers `WinTab`. Cheque o fabricante do seu dispositivo para instalar o driver WinTab apropriado.

-----

## Atualizando do Tahoma2D 1.2 ou Migrando do OpenToonz

Se estiver migrando do Tahoma2D 1.2 ou do OpenToonz, aqui estão algumas diferenças importantes de se mencionar:
- Alterações na Timeline/Xsheet
  - Células e Colunas não possuem a barra de arrastar no canto por padrão.
  - Se preferir o antigo comportamento, habilite `Preferências` -> `Cena` -> `Mostrar Barras de Arrastar Colunas e Células`.
  - Clique em uma Célula exposta ou em uma Coluna não vazia para arrastar e movê-la.
  - Selecione uma Célula/Coluna vazia e arraste para iniciar uma seleção de múltiplas Células/Colunas.
  - `ALT` agora substitui `CTRL` para selecionar Quadros e Chaves de animação.
  - `CTRL` agora inicia uma seleção de múltiplas Células/Colunas quando o primeiro clique for em uma Célula/Coluna exposta.
- Holds Implícitos
  - Não há necessidade para extender o Quadro manualmente. Um Quadro exposto será mostrado até o próximo Quadro exposto.
  - Use `Quadros de Parada` para terminar um hold sem expor outro Quadro.
  - Clique com o botão direito do mouse sobre qualquer Célula e selecione `Habilitar/Desabilitar Holds Implícitos` para mudar os modos.
  - Converta Cenas existentes de/para usar Holds Implícitos com `Cena` -> `Converter para Holds Implícitos/Explícitos`.
- Algumas funções e configurações adicionais do OpenToonz estão escondidas.
  - Habilite-as em `Preferências` -> `Geral` -> `Mostrar Preferências e Opções Avançadas*` e reinicie o Tahoma2D.

-----
## Dicas
- Animações e recursos externos (desenhos) de uma Cena são salvos em arquivos separados dentro de uma pasta de Projeto.
- Crie um Projeto `Tahoma2D` em sua pasta Documentos e use como seu projeto geral ao invés do padrão `sandbox`.
- Use `Salvar Tudo` (CTRL+S), regularmente, para salvar tanto a Cena quanto os recursos externos. `Salvar Cena` não salva mudanças nos recursos externos, apenas mudanças na Cena.
- Quando importar recursos externos, recomenda-se usar a opção `Importar` para manter o original intocado.
- Para múltiplos arquivos raster serem importados em 1 Nível, eles devem seguir a convenção de nome `NomeDoNível.####.ext` ou `NomeDoNível_####.ext`. (ex: ABC.0001.png, ABC.0002.png, etc...)
- Preste atenção na Barra de Status (parte inferior da tela) para informações adicionais de ferramentas e funções. 
- Precisa de mais ajuda? Se junte ao Discord do Tahoma2D ou outras redes sociais listadas em https://tahoma2d.org.