#!/bin/bash
rm package-lock.json
rm -rf node_modules

rm -rf vendor/tree-sitter
mkdir vendor/tree-sitter
cp -r ../tree-sitter/* vendor/tree-sitter

npm r tree-sitter
npm r tree-sitter-javascript
npm i

rm tree-sitter-0.20.0.tgz
npm pack

npm i tree-sitter-javascript-0.20.0.tgz

npx node-gyp --debug configure rebuild
