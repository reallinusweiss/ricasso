#ifndef SRE13_RBMATH_H
#define SRE13_RBMATH_H

struct lua_State;

/* Injects standard-compliant rbmath into _G.rbmath */
void sre13_inject_rbmath(struct lua_State* L);

#endif /* SRE13_RBMATH_H */
