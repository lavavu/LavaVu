#!/bin/sh
cd base
docker build -t base .
docker tag base lavavu/base:v0.1
docker push lavavu/base:v0.1
cd ..

cd latest
docker build -t latest .
docker tag latest lavavu/latest:v0.1
docker push lavavu/latest:v0.1

