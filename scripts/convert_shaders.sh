#!/bin/bash

#$1 resource_root

echo "convert yaml to json"
for i in `ls $1/shader/*.yaml`
do

new_filename="${i%.*}.json"
echo " $i -> $new_filename"

cat $i | scripts/yaml2json.py > $new_filename
done

echo "convert shaders to sesl"
for i in `ls $1/shader/*.json`
do

echo " $i"
flatc -b -o "$1/shader/" misc/ShaderComponent.fbs $i

done

echo "convert shader programms to sesp"
for i in `ls $1/shader_program/*.json`
do

echo " $i"
flatc -b -o "$1/shader_program/" misc/ShaderProgram.fbs $i

done
