FROM feliperaposo/protheus-docker-baselight:26
LABEL maintainer="Felipe Raposo <feliperaposo@gmail.com>"
COPY ./root/ /
CMD ["/protheus.sh"]
