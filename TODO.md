# incomplete TODO

- [ ] calculate minimum required quota to run a program
- [x] make global scope an optional language feature (so host apps can disable it before compilation)
- [ ] support iterator loops in language (optional feature that can be toggled)
- [ ] ensure SourceLocation is properly containing [begin, end] of the source code representation of the given AST
- [ ] ensure the AST always contains the right SourceLocation
- [ ] ensure SourceLocation is passed to IR (Value)
- [ ] ensure NativeCallback::Verifier has proper access to SourceLocation for debug info access
      (goal: properly implement an assume/assert API for use within native callbacks)
- [ ] flow: nested scopes with local variables must be initialized in this block
      currently also initialized in entry block;
      the problem is already how the AST is created.
- [ ] flow: eliminate unused variables (with side-effect-free initialization)
- [ ] flow: add sparse simple constant propagation SSCP

