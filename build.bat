@echo off

IF NOT EXIST bin mkdir bin

pushd bin
cl -W4 -nologo -Z7 -FC ../main.c
popd