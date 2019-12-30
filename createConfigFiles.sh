#! /bin/bash


if [[ -z "$1" ]]; then
  echo "Entrada vazia"
  echo "Padrão de chamada: ./createConfigFiles <nome>"
  echo "Nome -> sera o nome a ser colocado nos arquivos criados, nome sem ESPACO"
elif [[ -n "$1" ]]; then
  NAME=$1"_"
    BRANCH="branch"
    PREF="pref_"
    REPL="repl_"
    l1d="l1d"
    l1i="l1i"
    l2c="l2c"
    llc="llc"

    cp branch/branch_predictor.cc branch/$NAME$BRANCH.bpred
    echo $NAME$BRANCH" done"
    cp prefetcher/l1d_prefetcher.cc prefetcher/$NAME$PREF$l1d.l1d_pref
    echo $NAME$PREF$l1d" done"
    cp prefetcher/l1i_prefetcher.cc prefetcher/$NAME$PREF$l1i.l1i_pref
    echo $NAME$PREF$l1i" done"
    cp prefetcher/l2c_prefetcher.cc prefetcher/$NAME$PREF$l2c.l2c_pref
    echo $NAME$PREF$l2c" done"
    cp prefetcher/llc_prefetcher.cc prefetcher/$NAME$PREF$llc.llc_pref
    echo $NAME$PREF$llc" done"
    cp replacement/llc_replacement.cc replacement/$NAME$REPL$llc.llc_repl
    echo $NAME$REPL$llc" done"



    echo "DONE"
fi



