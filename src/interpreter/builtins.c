#include <assert.h>
#include <stdarg.h>
#include "../ast.h"
#include "internal.h"
#include "../stringify.h"
#include "../util.h"
#include "./builtins.h"

// TODO: better way for range
#define EXPECT(cond) do { \
	if (!(nargs cond)) { \
		return rr_errf("expected nargs (%d) to be %s", nargs, #cond); \
	} \
} while(0)

#define EXPECT_TYPE(narg, typ) do { \
	if (args[(narg)]->type != (typ)) { \
		return rr_errf("expected argument %d to be of type %s", narg, #typ); \
	} \
} while(0)

RunResult builtin_print(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	(void)name;
	EXPECT(>= 1);

	for (size_t i = 0; i < nargs; i++) {
		if (i != 0) {
			putchar(' ');
		}

		const Node *arg = args[i];
		assert(arg);

		RunResult rr = run(env, arg);
		if (rr.err != NULL) {
			return rr;
		}
		arg = rr.node;

		char *str = toString(arg);
		node_free(rr.node);
		printf("%s", str);
		free(str);
	}

	putchar('\n'); // REVIEW: do we want to do this?
	return rr_null();
}

RunResult builtin_arith(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	EXPECT(== 2);

	Node *res = malloc(1, sizeof(Node));
	res->type = AST_NUM;

	RunResult rr1 = run(env, args[0]);
	RunResult rr2 = run(env, args[1]);

	if (
		(rr1.node->type != AST_NUM) ||
		(rr2.node->type != AST_NUM)
	) {
		return rr_errf("both arguments should be a number");
	}

	double n1 = rr1.node->num.val;
	double n2 = rr2.node->num.val;

	const char symbol = name[0];
	switch (symbol) {
	case '+':
		res->num.val = n1 + n2;
		break;
	case '-':
		res->num.val = n1 - n2;
		break;
	case '/':
		res->num.val = n1 / n2;
		break;
	case '*':
		res->num.val = n1 * n2;
		break;
	}

	return rr_node(res);
}

RunResult builtin_comp(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	EXPECT(== 2);

	Node *res = malloc(1, sizeof(Node));
	res->type = AST_NUM;

	double n1 = getNumVal(env, args[0]);
	double n2 = getNumVal(env, args[1]);

	if (streq(name, "eq")) {
		res->num.val = n1 == n2;
	} else if (streq(name, "neq")) {
		res->num.val = n1 != n2;
	} else if (streq(name, "lt")) {
		res->num.val = n1 < n2;
	} else if (streq(name, "gt")) {
		res->num.val = n1 > n2;
	}

	return rr_node(res);
}

RunResult builtin_do(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	(void)name;

	EXPECT(>= 1);

	RunResult rr;
	for (size_t i = 0; i < nargs; i++) {
		rr = run(env, args[i]);
		if (rr.err != NULL) {
			return rr;
		}
	}

	return rr;
}

RunResult builtin_if(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	(void)name;

	EXPECT(>= 2);
	EXPECT(<= 3);

	RunResult cond_rr = run(env, args[0]);
	if (cond_rr.err != NULL) {
		return cond_rr;
	} else if (cond_rr.node->type != AST_NUM) {
		return rr_errf("expected cond to have type AST_NUM");
	}

	if (cond_rr.node->num.val) {
		return run(env, args[1]);
	} else if (nargs == 3) {
		return run(env, args[2]);
	}

	return rr_null();
}

RunResult builtin_set(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	(void)name;

	EXPECT(== 2);
	EXPECT_TYPE(0, AST_VAR);

	RunResult rr = run(env, args[1]);
	if (rr.err != NULL) {
		return rr;
	}

	if (rr.node == NULL) {
		varmap_removeItem(env->variables, args[0]->var.name);
	} else {
		varmap_setItem(env->variables, args[0]->var.name, rr.node);
	}

	return rr_null();
}

RunResult builtin_let(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	(void)name;

	EXPECT(>= 3);

	builtin_set(env, "set", 2, args);

	RunResult rr;
	for (size_t i = 2; i < nargs; i++) {
		rr = run(env, args[i]);
		if (rr.err != NULL) {
			return rr;
		}
	}

	varmap_removeItem(env->variables, args[0]->var.name);

	return rr;
}

RunResult builtin_streq(InterEnv *env, const char *name, size_t nargs, const Node **args) {
	(void)name;

	EXPECT(== 2);

	Node *res = malloc(1, sizeof(Node));
	res->type = AST_NUM;

	RunResult rr1 = run(env, args[0]);
	RunResult rr2 = run(env, args[1]);

	if (
		(rr1.node->type != AST_STR) ||
		(rr2.node->type != AST_STR)
	) {
		return rr_errf("both arguments should be a string");
	}

	res->num.val = (
		rr1.node->str.str == rr2.node->str.str ||
		streq(rr1.node->str.str, rr2.node->str.str)
	);

	return rr_node(res);
}

#define STATIC_BUILTIN_COUNT 14
Builtin staticbuiltins[STATIC_BUILTIN_COUNT] = {
	{
		.name = "print",
		.enabled = true,
		.fn = builtin_print,
	}, {
		.name = "+",
		.enabled = true,
		.fn = builtin_arith,
	}, {
		.name = "-",
		.enabled = true,
		.fn = builtin_arith,
	}, {
		.name = "/",
		.enabled = true,
		.fn = builtin_arith,
	}, {
		.name = "*",
		.enabled = true,
		.fn = builtin_arith,
	}, {
		.name = "eq",
		.enabled = true,
		.fn = builtin_comp,
	}, {
		.name = "neq",
		.enabled = true,
		.fn = builtin_comp,
	}, {
		.name = "lt",
		.enabled = true,
		.fn = builtin_comp,
	}, {
		.name = "gt",
		.enabled = true,
		.fn = builtin_comp,
	}, {
		.name = "do",
		.enabled = true,
		.fn = builtin_do,
	}, {
		.name = "if",
		.enabled = true,
		.fn = builtin_if,
	}, {
		.name = "set",
		.enabled = true,
		.fn = builtin_set,
	}, {
		.name = "let",
		.enabled = true,
		.fn = builtin_let,
	}, {
		.name = "streq",
		.enabled = true,
		.fn = builtin_streq,
	}
};

Builtin *getBuiltin(const char *name) {
	for (size_t i = 0; i < STATIC_BUILTIN_COUNT; i++) {
		Builtin builtin = staticbuiltins[i];
		if (streq(name, builtin.name)) {
			return staticbuiltins + i; // HACK
		}
	}

	return NULL;
}

/*
void in_enable_builtin(const char *builtin, bool val) {
	if (builtin == NULL) {
		for (size_t i = 0; i < BUILTIN_COUNT; i++) {
			Builtin b = builtins[i];
			in_enable_builtin(b.name, val);
		}
	}

	for (size_t i = 0; i < BUILTIN_COUNT; i++) {
		Builtin b = builtins[i];
		if (streq(b.name, builtin)) {
			b.enabled = val;
		}
	}
}
*/
