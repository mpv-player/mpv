
class ParseError(Exception):
    pass

class AstOp(object):
    def __init__(self, op, sub):
        self.op = op
        self.sub = sub

    def __repr__(self):
        if len(self.sub) == 1:
            return self.op + str(self.sub[0])
        return "(" + (" " + self.op + " ").join([str(x) for x in self.sub]) + ")"

class AstSym(object):
    def __init__(self, name):
        assert type(name) is type("")
        self.name = name

    def __repr__(self):
        return self.name

Arity = { "!": 1, "&&": 2, "||": 2 }
Precedence = { "!": 3, "&&": 2, "||": 1 }
Tokens = list(Arity.keys()) + ["(", ")"]

# return (token, rest), or (None, "") if nothing left
def read_tok(expr):
    expr = expr.strip()
    for t in Tokens:
        if expr.startswith(t):
            return (t, expr[len(t):])
    if expr == "":
        return (None, "")
    sym = ""
    while len(expr) and ((expr[0].lower() >= 'a' and expr[0].lower() <= 'z') or
                         (expr[0] >= '0' and expr[0] <= '9') or
                         (expr[0] in ["_", "-", "."])):
        sym += expr[0]
        expr = expr[1:]
    if len(sym):
        return sym, expr
    raise ParseError("unknown token in '%s'" % expr)

def parse_expr(expr):
    opstack = []
    outstack = []
    def out(sym):
        if sym in Arity:
            sub = []
            for i in range(Arity[sym]):
                if len(outstack) == 0:
                    raise ParseError("missing operator argument")
                sub.insert(0, outstack.pop())
            outstack.append(AstOp(sym, sub))
        elif sym == "(":
            raise ParseError("missing closing ')'")
        elif not isinstance(sym, AstSym):
            raise ParseError("bogus symbol '%s'" % sym)
        else:
            outstack.append(sym)
    while True:
        tok, expr = read_tok(expr)
        if tok is None:
            break
        if tok in Arity:
            while len(opstack) and opstack[-1] != '(' and \
                  Precedence[opstack[-1]] > Precedence[tok]:
                out(opstack.pop())
            opstack.append(tok)
        elif tok == "(":
            opstack.append(tok)
        elif tok == ")":
            while True:
                if not len(opstack):
                    raise ParseError("missing '(' for ')'")
                sym = opstack.pop()
                if sym == "(":
                    break
                out(sym)
        else:
            out(AstSym(tok)) # Assume a terminal
    while len(opstack):
        out(opstack.pop())
    if len(outstack) != 1:
        raise ParseError("empty expression or extra symbols (%s)" % outstack)
    return outstack.pop()

def convert_dnf(ast):

    # no nested ! (negation normal form)
    def simplify_negation(ast):
        if isinstance(ast, AstOp):
            if ast.op == "!":
                sub = ast.sub[0]
                if isinstance(sub, AstOp):
                    if sub.op == "!":
                        return sub.sub[0]
                    elif sub.op in ["&&", "||"]:
                        sub.op = "||" if sub.op == "&&" else "&&"
                        sub.sub = [AstOp("!", [x]) for x in sub.sub]
                        return simplify_negation(sub)
            else:
                ast.sub = [simplify_negation(x) for x in ast.sub]
        return ast

    # a && (b && c) => a && b && c
    def flatten(ast):
        if isinstance(ast, AstOp):
            can_flatten = ast.op in ["&&", "||"]
            nsub = []
            for sub in ast.sub:
                sub = flatten(sub)
                if isinstance(sub, AstOp) and sub.op == ast.op and can_flatten:
                    nsub.extend(sub.sub)
                else:
                    nsub.append(sub)
            ast.sub = nsub
            if len(ast.sub) == 1 and can_flatten:
                return ast.sub[0]
        return ast

    # a && (b || c) && d => (a && d && b) || (a && d && c)
    def redist(ast):
        def recombine(a, stuff):
            return AstOp("||", [AstOp("&&", [a, n]) for n in stuff])
        if isinstance(ast, AstOp):
            ast.sub = [flatten(redist(x)) for x in ast.sub]
            if ast.op == "&&":
                for sub in ast.sub:
                    if isinstance(sub, AstOp) and sub.op == "||":
                        if len(ast.sub) == 1:
                            return redist(sub)
                        other = None
                        for n in ast.sub:
                            if n is not sub:
                                if other is None:
                                    other = n
                                else:
                                    other = flatten(AstOp("&&", [other, n]))
                        return flatten(redist(recombine(other, sub.sub)))
        return ast

    return redist(flatten(simplify_negation(ast)))

# Returns (success_as_bool, failure_reason_as_string)
def check_dependency_expr(expr, deps):
    ast = parse_expr(expr)
    def eval_ast(ast):
        if isinstance(ast, AstSym):
            return ast.name in deps
        elif isinstance(ast, AstOp):
            vals = [eval_ast(x) for x in ast.sub]
            if ast.op == "&&":
                return vals[0] and vals[1]
            elif ast.op == "||":
                return vals[0] or vals[1]
            elif ast.op == "!":
                return not vals[0]
        assert False
    if eval_ast(ast):
        return True, None

    # Now the same thing again, but more complicated, and informing what is
    # missing.
    ast = convert_dnf(ast)

    # ast now is a or-combined list of and-combined deps. Each dep can have a
    # negation (marking a conflict). Each case of and-combined deps is a way
    # to satisfy the deps expression. Instead of dumping full information,
    # distinguish the following cases, and only mention the one that applies,
    # in order:
    #   1. the first missing dep of a case that has missing deps only
    #   2. the first conflicting dep at all

    def get_sub_list(node, op):
        if isinstance(node, AstOp) and node.op == op:
            return node.sub
        else:
            return [node]

    conflict_dep = None
    missing_dep = None

    for group in get_sub_list(ast, "||"):
        group_conflict = None
        group_missing_dep = None
        for elem in get_sub_list(group, "&&"):
            neg = False
            if isinstance(elem, AstOp) and elem.op == "!":
                neg = True
                elem = elem.sub[0]
            if not isinstance(elem, AstSym):
                continue # broken DNF?
            name = elem.name
            present = name in deps
            if (not present) and (not neg) and (group_missing_dep is None):
                group_missing_dep = name
            if present and neg and (group_conflict is None):
                group_conflict = name
        if (missing_dep is None) and (group_conflict is None):
            missing_dep = group_missing_dep
        if conflict_dep is None:
            conflict_dep = group_conflict

    reason = "unknown"
    if missing_dep is not None:
        reason = "%s not found" % (missing_dep)
    elif conflict_dep is not None:
        reason = "%s found" % (conflict_dep)
    return False, reason
