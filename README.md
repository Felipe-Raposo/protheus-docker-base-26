# Protheus Docker Base 26.x.y.z (Arara Azul)

Imagem Docker base para execução do **TOTVS Protheus 12** (AppServer) em ambiente containerizado.

## Descrição

Este projeto monta a imagem `feliperaposo/protheus-docker-base:26`, usada pelo Protheus 12.1.2610. Ela estende [protheus-docker-baselight:26](https://github.com/Felipe-Raposo/protheus-docker-baselight-26) com `appserver.ini`, SmartClient WebApp, Web Agent e o proxy da porta pública.

Se estiver buscando um ambiente completo com Protheus + License + dbAccess + Postgres, acesse esse [projeto](https://github.com/Felipe-Raposo/protheus-docker-compose).

### Características

- **Base:** `feliperaposo/protheus-docker-baselight:26` (Oracle Linux 9 Slim + AppServer 26.3.0.0)
- **AppServer:** porta TCP 1235
- **Proxy:** `app-root-proxy` em `/usr/local/bin/`, na porta 1234. Encaminha `/app-root` ao HTTP Server e o restante ao AppServer
- **HTTP Server:** porta 9995 (`/app-root`, raiz em `/protheus12/protheus_data/http-root/app-root`)
- **DBAPI:** `dbapi.so` baixado de `http://dbaccess:7891/dbapi.so` para `/protheus12/bin/appserver/` na inicialização, quando o `appserver.ini` tem `[DBAccess]` (`DBAPI_URL` substitui a URL)
- **Printer:** `printer` em `/protheus12/bin/appserver/`
- **SmartClient WebApp:** `webapp.so` em `/protheus12/bin/appserver/`
- **Web Agent:** 1.1.2 (instaladores deb/rpm/dmg/exe em `/protheus12/bin/webagent/`)
- **REST:** porta 8080 (`/rest`)
- **App Monitor:** porta 32033 (`/app-monitor`)
- **Configuração dinâmica:** banco de dados definido via `APPSERVER_DATABASE`

## Pré-requisitos

- Docker (com BuildKit habilitado para build)
- Host de build `linux/amd64` (`make build` passa `--platform=linux/amd64`)
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
- **AppServer:** binários do baselight em `/protheus12/bin/appserver/`; overlay desta imagem: `appserver.ini`, `printer`, `webapp.so`. O `dbapi.so` vem do dbAccess na inicialização
- **Proxy:** `/usr/local/bin/app-root-proxy`, iniciado pelo entrypoint nas portas 1234, 1235 e 9995
- **Web Agent:** `/protheus12/bin/webagent/`
- **Configuração:** `appserver.ini` (parâmetro `{{APPSERVER_DATABASE}}` substituído em runtime)
- **Protheus Data:** `/protheus12/protheus_data/`
- **SystemLoad:** Descompactação automática de `systemload.tar.xz` em `/protheus12/protheus_data/systemload` na primeira inicialização

## Licença

Este projeto está sob a [GNU General Public License v3.0](LICENSE).

## Mantenedor

Felipe Raposo — [feliperaposo@gmail.com](mailto:feliperaposo@gmail.com)
