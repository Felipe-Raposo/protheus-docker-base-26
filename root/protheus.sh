#!/bin/bash

export LC_ALL=C
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.

# Pacotes gerados no macOS trazem xattrs (com.apple.*) e arquivos AppleDouble (._*).
TAR_OPTS=(--warning=no-unknown-keyword --exclude='._*')

# Descompacta a SystemLoad em background.
if [[ -f "/protheus12/protheus_data/systemload.tar.xz" ]]; then
	cd /protheus12/protheus_data/
	(tar "${TAR_OPTS[@]}" -xvf systemload.tar.xz && rm systemload.tar.xz) &
fi

# Descompacta o binário.
if [[ -f "/protheus12/bin/appserver.tar.xz" ]]; then
	cd /protheus12/bin/
	tar "${TAR_OPTS[@]}" -xvf appserver.tar.xz
	rm appserver.tar.xz
fi

# Configura e inicia serviço do Protheus.
cd /protheus12/bin/appserver/
mkdir -p ./app-root
sed 's/{{APPSERVER_DATABASE}}/'"${APPSERVER_DATABASE}"'/' -i ./appserver.ini
echo "Inicializando Protheus AppServer $(./appsrvlinux --version) ${APPSERVER_DATABASE}"
# exec: appsrvlinux assume o PID 1 e recebe SIGTERM do "docker stop" (shutdown gracioso).
exec ./appsrvlinux
