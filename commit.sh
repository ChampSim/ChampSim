#! /bin/bash



if [[ -z "$1" ]]; then
  echo "Entrada vazia"
  echo "Padrão de chamada: ./commit <mensagem do commit>"
elif [[ -n "$1" ]]; then
    MENSAGEM= $1
    git add *
    git commit -m $MENSAGEM
    git push origin alff
fi