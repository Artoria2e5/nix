let {

  inherit (import ../../../lib) compileC link;

  inherit (import ../aterm) libATerm;

  compile = fn: compileC {
    main = fn;
    localIncludes = "auto";
    cFlags = "-I../aterm";
  };

  fib = link {objects = compile ./fib.c; libraries = libATerm;};

  body = [fib];
}
