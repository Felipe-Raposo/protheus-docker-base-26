# Protheus Docker Base 26.x.y.z (Arara Azul)

Imagem Docker base para execução do **TOTVS Protheus 12** (AppServer) em ambiente containerizado.

## Descrição

Este projeto monta uma imagem base Docker para rodar o AppServer do Protheus 12 sobre Oracle Linux 9. A imagem estende [protheus-docker-baselight:26](https://hub.docker.com/r/feliperaposo/protheus-docker-baselight) com configuração padrão, SmartClient WebApp e Web Agent.

Esse projeto gera imagem base para a construção de imagens de AppServer para License e para Protheus 12.1.2410 ou 12.1.2510.

### Características

- **Base:** `feliperaposo/protheus-docker-baselight:26` (Oracle Linux 9 Slim + AppServer 26.x.y.z)
- **Protheus:** AppServer (porta TCP 1234)
- **DBAPI:** `dbapi.so` em `/protheus12/bin/appserver/`
- **Printer:** `printer` em `/protheus12/bin/appserver/`
- **SmartClient WebApp:** `webapp.so` em `/protheus12/bin/appserver/`
- **Web Agent:** 1.1.2 (instaladores deb/rpm/dmg/exe em `/protheus12/bin/webagent/`)
- **REST API:** Habilitada na porta 8080 (`/rest`)
- **Configuração dinâmica:** Banco de dados definido via variável de ambiente `APPSERVER_DATABASE`

## Pré-requisitos

- Docker (com BuildKit habilitado para build)
- Imagem `feliperaposo/protheus-docker-baselight:26` disponível localmente ou no registry

## Build

```bash
# Build local das tags de versão
make build
```

## Publicação da imagem

```bash
# Build e push das tags de versão
make release

# Incluir também a tag latest
make release_latest
```

Gera as tags:

- `feliperaposo/protheus-docker-base:26`
- `feliperaposo/protheus-docker-base:26.3.0.0`
- `feliperaposo/protheus-docker-base:latest`

## Estrutura interna (referência)

- **Entrypoint:** `/protheus.sh`
- **APO:** `/protheus12/apo/`
- **AppServer:** Binários do baselight em `/protheus12/bin/appserver/`; overlay desta imagem: `appserver.ini`, `dbapi.so`, `printer`, `webapp.so`
- **Web Agent:** `/protheus12/bin/webagent/`
- **Configuração:** `appserver.ini` (parâmetro `{{APPSERVER_DATABASE}}` substituído em runtime)
- **Protheus Data:** `/protheus12/protheus_data/`
- **SystemLoad:** Descompactação automática de `systemload.tar.xz` em `/protheus12/protheus_data/systemload` na primeira inicialização

## Licença

Este projeto está sob a [GNU General Public License v3.0](LICENSE).

## Mantenedor

Felipe Raposo — [feliperaposo@gmail.com](mailto:feliperaposo@gmail.com)
