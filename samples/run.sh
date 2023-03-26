# !/bin/bash
cp ../dot/header.dot .
clang -S -emit-llvm ${1}.c -o ${1}.ll

opt -enable-new-pm=0 -S -load ../build/pass/libllvm_dot_pass.so --dotpass < ${1}.ll -o after-${1}.ll >> header.dot
echo "}" >> header.dot
dot -Tpng header.dot -o graph.png

clang after-${1}.ll dyn_instr.c -o ${1}.exe
