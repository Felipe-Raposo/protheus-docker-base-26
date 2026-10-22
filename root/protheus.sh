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

# O MPP publica /app-root nesta pasta. Sem ela a location é recusada.
mkdir -p /protheus12/protheus_data/http-root/app-root

# O AppServer escuta na 1235. A 1234 fica com o proxy, que entrega /app-root
# (tela seguinte ao login do WebApp) a partir do HTTPSERVER na 9995.
if [[ -x /usr/local/bin/app-root-proxy ]]; then
	/usr/local/bin/app-root-proxy 1234 1235 9995 &
fi

# Configura e inicia serviço do Protheus.
cd /protheus12/bin/appserver/

# O dbapi.so acompanha a versão do dbAccess. O License Server não tem [DBAccess].
if grep -q '^\[DBAccess\]' ./appserver.ini; then
	dbapi_url="${DBAPI_URL:-http://dbaccess:7891/dbapi.so}"
	dbapi_tmp=$(mktemp /protheus12/bin/appserver/dbapi.so.XXXXXX)
	echo "Baixando dbapi.so de ${dbapi_url}"
	if ! curl --retry 30 --retry-delay 2 --retry-connrefused --retry-all-errors \
		-fsSL -o "$dbapi_tmp" "$dbapi_url"; then
		rm -f "$dbapi_tmp"
		echo "Falha ao baixar dbapi.so de ${dbapi_url}" >&2
		exit 1
	fi
	chmod 755 "$dbapi_tmp"
	mv -f "$dbapi_tmp" /protheus12/bin/appserver/dbapi.so
fi

sed 's/{{APPSERVER_DATABASE}}/'"${APPSERVER_DATABASE}"'/' -i ./appserver.ini
echo "Inicializando Protheus AppServer $(./appsrvlinux --version) ${APPSERVER_DATABASE}"
# exec: appsrvlinux assume o PID 1 e recebe SIGTERM do "docker stop" (shutdown gracioso).
exec ./appsrvlinux
