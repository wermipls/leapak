#!/bin/bash

set -e

cd "$(dirname "$(readlink -f -- "$0" || realpath -- "$0")")"

pak=../pak
unpak=../unpak
if [[ $1 == 'validate' ]]; then
    validate=1
fi

function test_unpak () {
    local original=$1
    local packed=$2
    local out_unpak=.tmp_unpacked

    $unpak "$packed" "$out_unpak" > /dev/null
    cmp "$original" "$out_unpak"
    rm "$out_unpak"
}

function dirsize () {
    du -sb "$1" | perl -ne 'm/(^[0-9]+)/ && print "$1"'
}

dir=cantrbry
files=$dir/*

outdir="output"
mkdir -p "$outdir"

for f in $files
do
    name=`basename "$f"`
    echo $f
    outfile="$outdir/$name.leapak"
    $pak "$f" "$outfile" > /dev/null
    if [[ $validate == 1 ]]; then
        test_unpak "$f" "$outfile"
    fi
done

echo `dirsize "$dir"` "unpacked" 
echo `dirsize "$outdir"` "packed"