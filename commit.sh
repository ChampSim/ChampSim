#! /bin/bash


if [[ -z "$1" ]]; then
  echo "Entrada vazia"
  echo "Padrão de chamada: ./commit <mensagem do commit>"
elif [[ -n "$1" ]]; then
    git add *
    git commit -m $1
    git push origin Alff
fi