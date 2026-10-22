FROM feliperaposo/protheus-docker-baselight:26
LABEL maintainer="Felipe Raposo <feliperaposo@gmail.com>"
COPY ./root/ /
# O dbapi.so do baselight fica defasado em relação ao dbAccess. O entrypoint baixa a cópia certa.
RUN rm -f /protheus12/bin/appserver/dbapi.so
CMD ["/protheus.sh"]
