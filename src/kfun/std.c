# ifndef FUNCDEF
# include "kfun.h"
# include "path.h"
# include "comp.h"
# include "comm.h"
# endif


# ifdef FUNCDEF
FUNCDEF("call_other", kf_call_other, p_call_other)
# else
char p_call_other[] = { C_STATIC | C_LOCAL | C_VARARGS, T_MIXED, 32,
			T_MIXED, T_STRING,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED,
			T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED };

/*
 * NAME:	kfun->call_other()
 * DESCRIPTION:
 */
int kf_call_other(nargs)
int nargs;
{
    register object *o;
    register value *val;

    if (nargs < 2) {
	error("Too few arguments to call_other");
    }

    val = &sp[nargs - 1];
    switch (val->type) {
    case T_STRING:
	o = o_find(path_object(val->u.string->text, o_name(i_this_object())));
	if (o == (object *) NULL) {
	    o = c_compile(val->u.string->text);
	}
	str_del(val->u.string);
	break;

    case T_OBJECT:
	o = o_object(&val->u.object);
	break;

    default:
	return 1;
    }

    val->type = T_NUMBER;
    val->u.number = 0;
    --val;
    if (val->type != T_STRING) {
	return 2;
    }

    if (i_apply(o, val->u.string->text, FALSE, nargs - 2)) {
	val = sp++;
	str_del((sp++)->u.string);
	*sp = *val;
    } else if (nargs > 2) {
	/*
	 * Hmm...
	 */
	error("Call_other to non-existing function %s", sp->u.string->text);
    } else {
	str_del((sp++)->u.string);
    }
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("this_object", kf_this_object, p_this_object)
# else
char p_this_object[] = { C_STATIC | C_LOCAL, T_OBJECT, 0 };

/*
 * NAME:	kfun->this_object()
 * DESCRIPTION:	return the current object
 */
int kf_this_object()
{
    register object *obj;
    value val;

    obj = i_this_object();
    val.type = T_OBJECT;
    val.u.object = obj->key;
    i_push_value(&val);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("previous_object", kf_previous_object, p_previous_object)
# else
char p_previous_object[] = { C_STATIC | C_LOCAL, T_OBJECT, 0 };

/*
 * NAME:	kfun->previous_object()
 * DESCRIPTION:	return the previous object
 */
int kf_previous_object()
{
    register object *obj;
    value val;

    obj = i_prev_object();
    val.type = T_OBJECT;
    val.u.object = obj->key;
    i_push_value(&val);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("error", kf_error, p_error)
# else
char p_error[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 1, T_STRING };

/*
 * NAME:	kfun->error()
 * DESCRIPTION:
 */
int kf_error()
{
    error("%s", sp->u.string->text);
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("send_message", kf_send_message, p_send_message)
# else
char p_send_message[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_VOID, 1,
			  T_STRING };

/*
 * NAME:	kfun->send_message()
 * DESCRIPTION:
 */
int kf_send_message()
{
    comm_send(sp->u.string, i_this_object());
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("allocate", kf_allocate, p_allocate)
# else
char p_allocate[] = { C_TYPECHECKED | C_STATIC | C_LOCAL,
		      T_MIXED | (1 << REFSHIFT), 1, T_NUMBER };

/*
 * NAME:	kfun->allocate()
 * DESCRIPTION:
 */
int kf_allocate()
{
    register int i;
    register value *val;

    arr_ref(sp->u.array = arr_new((long) sp->u.number));
    for (i = sp->u.array->size, val = sp->u.array->elts; i > 0; --i, val++) {
	val->type = T_NUMBER;
	val->u.number = 0;
    }
    sp->type = T_ARRAY;
    return 0;
}
# endif


# ifdef FUNCDEF
FUNCDEF("sizeof", kf_sizeof, p_sizeof)
# else
char p_sizeof[] = { C_TYPECHECKED | C_STATIC | C_LOCAL, T_NUMBER, 1,
		    T_MIXED | (1 << REFSHIFT) };

/*
 * NAME:	kfun->sizeof()
 * DESCRIPTION:
 */
int kf_sizeof()
{
    int i;

    i = sp->u.array->size;
    arr_del(sp->u.array);
    sp->type = T_NUMBER;
    sp->u.number = i;
    return 0;
}
# endif


/*
arrayp()
clone_object()
destruct()
file_name()
find_object()
function_exists()
intp()
m_indices()
m_sizeof()
m_values()
mappingp()
objectp()
query_ip_number()
shutdown()
stringp()
strlen()
this_user()
time()
users()
*/