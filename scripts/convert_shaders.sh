#!/bin/bash

#$1 resource_root
#$2 path to scheme files (*.fbs)

echo "convert yaml to json"
for i in `ls $1/shader/*.yaml`
do

new_filename="${i%.*}.json"
echo " $i -> $new_filename"

cat $i | $2/scripts/yaml2json.py > $new_filename
done

echo "convert shaders to sesl"
for i in `ls $1/shader/*.json`
do

echo " $i"
flatc -b -o "$1/shader/" $2/misc/ShaderComponent.fbs $i
done

echo "convert shader programms to sesp"
for i in `ls $1/shader_program/*.json`
do

echo " $i"
flatc -b -o "$1/shader_program/" -I "`realpath $2/misc/`" "$2/misc/ShaderProgram.fbs" $i
done


echo "convert materials to semt"
for i in `ls $1/material/*.json`
do

echo " $i"
flatc -b -o "$1/material/" -I "`realpath $2/misc/`" "$2/misc/Material.fbs" $i

done
