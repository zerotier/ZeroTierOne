#pragma once

#include "datum.h"
#include "connection.h"
#include "protocol_defs.h"
#include "cursor.h"

namespace RethinkDB {

using TT = Protocol::Term::TermType;

class Term;
class Var;

// An alias for the Term constructor
template <class T>
Term expr(T&&);

int gen_var_id();

// Can be used as the last argument to some ReQL commands that expect named arguments
using OptArgs = std::map<std::string, Term>;

// Represents a ReQL Term (RethinkDB Query Language)
// Designed to be used with r-value *this
class Term {
public:
    Term(const Term& other) = default;
    Term(Term&& other) = default;
    Term& operator= (const Term& other) = default;
    Term& operator= (Term&& other) = default;

    explicit Term(Datum&&);
    explicit Term(const Datum&);
    explicit Term(OptArgs&&);

    // Create a copy of the Term
    Term copy() const;

    Term(std::function<Term()> f) : datum(Nil()) { set_function<std::function<Term()>>(f); }
    Term(std::function<Term(Var)> f) : datum(Nil()) { set_function<std::function<Term(Var)>, 0>(f); }
    Term(std::function<Term(Var, Var)> f) : datum(Nil()) { set_function<std::function<Term(Var, Var)>, 0, 1>(f); }
    Term(std::function<Term(Var, Var, Var)> f) : datum(Nil()) { set_function<std::function<Term(Var, Var, Var)>, 0, 1, 2>(f); }
    Term(Protocol::Term::TermType type, std::vector<Term>&& args) : datum(Array()) {
        Array dargs;
        for (auto& it : args) {
            dargs.emplace_back(alpha_rename(std::move(it)));
        }
        datum = Datum(Array{ type, std::move(dargs) });
    }

    Term(Protocol::Term::TermType type, std::vector<Term>&& args, OptArgs&& optargs) : datum(Array()) {
        Array dargs;
        for (auto& it : args) {
            dargs.emplace_back(alpha_rename(std::move(it)));
        }
        Object oargs;
        for (auto& it : optargs) {
            oargs.emplace(it.first, alpha_rename(std::move(it.second)));
        }
        datum = Array{ type, std::move(dargs), std::move(oargs) };
    }

    // Used internally to support row
    static Term func_wrap(Term&&);
    static Term func_wrap(const Term&);


    // These macros are used to define most ReQL commands
    //  * Cn represents a method with n arguments
    //  * COn represents a method with n arguments and optional named arguments
    //  * C_ represents a method with any number of arguments
    // Each method is implemented twice, once with r-value *this, and once with const *this
    // The third argument, wrap, allows converting arguments into functions if they contain row

#define C0(name, type) \
    Term name() &&      { return Term(TT::type, std::vector<Term>{ std::move(*this) }); } \
    Term name() const & { return Term(TT::type, std::vector<Term>{ *this            }); }
#define C1(name, type, wrap)                                            \
    template <class T>                                                  \
    Term name(T&& a) && { return Term(TT::type, std::vector<Term>{ std::move(*this), wrap(expr(std::forward<T>(a))) }); } \
    template <class T>                                                  \
    Term name(T&& a) const & { return Term(TT::type, std::vector<Term>{ *this, wrap(expr(std::forward<T>(a))) }); }
#define C2(name, type)                                                  \
    template <class T, class U> Term name(T&& a, U&& b) && {           \
        return Term(TT::type, std::vector<Term>{ std::move(*this),    \
                    expr(std::forward<T>(a)), expr(std::forward<U>(b)) }); } \
    template <class T, class U> Term name(T&& a, U&& b) const & {      \
        return Term(TT::type, std::vector<Term>{ *this,        \
                    expr(std::forward<T>(a)), expr(std::forward<U>(b)) }); }
#define C_(name, type, wrap)                                            \
    template <class ...T> Term name(T&& ...a) && {                     \
        return Term(TT::type, std::vector<Term>{ std::move(*this),    \
                    wrap(expr(std::forward<T>(a)))... }); }             \
    template <class ...T> Term name(T&& ...a) const & {                \
        return Term(TT::type, std::vector<Term>{ *this,        \
                    wrap(expr(std::forward<T>(a)))... }); }
#define CO0(name, type)                                                 \
    Term name(OptArgs&& optarg = {}) && {                              \
        return Term(TT::type, std::vector<Term>{ std::move(*this) }, std::move(optarg)); } \
    Term name(OptArgs&& optarg = {}) const & {                         \
        return Term(TT::type, std::vector<Term>{ *this }, std::move(optarg)); }
#define CO1(name, type, wrap)                                                \
    template <class T> Term name(T&& a, OptArgs&& optarg = {}) && {    \
        return Term(TT::type, std::vector<Term>{ std::move(*this),    \
                    wrap(expr(std::forward<T>(a))) }, std::move(optarg)); } \
    template <class T> Term name(T&& a, OptArgs&& optarg = {}) const & { \
        return Term(TT::type, std::vector<Term>{ *this,               \
                    wrap(expr(std::forward<T>(a))) }, std::move(optarg)); }
#define CO2(name, type, wrap)                                           \
    template <class T, class U> Term name(T&& a, U&& b, OptArgs&& optarg = {}) && { \
        return Term(TT::type, std::vector<Term>{ std::move(*this),    \
                    wrap(expr(std::forward<T>(a))), wrap(expr(std::forward<U>(b))) }, std::move(optarg)); } \
    template <class T, class U> Term name(T&& a, U&& b, OptArgs&& optarg = {}) const & { \
        return Term(TT::type, std::vector<Term>{ *this,        \
                    wrap(expr(std::forward<T>(a))), wrap(expr(std::forward<U>(b))) }, std::move(optarg)); }
#define CO3(name, type, wrap)                                                \
    template <class T, class U, class V> Term name(T&& a, U&& b, V&& c, OptArgs&& optarg = {}) && { \
        return Term(TT::type, std::vector<Term>{ std::move(*this),    \
                    wrap(expr(std::forward<T>(a))), wrap(expr(std::forward<U>(b))), \
                    wrap(expr(std::forward<V>(c))) }, std::move(optarg)); } \
    template <class T, class U, class V> Term name(T&& a, U&& b, V&& c, OptArgs&& optarg = {}) const & { \
        return Term(TT::type, std::vector<Term>{ *this,        \
                    wrap(expr(std::forward<T>(a))), wrap(expr(std::forward<U>(b))), \
                    wrap(expr(std::forward<V>(c)))}, std::move(optarg)); }
#define CO4(name, type, wrap)                                                \
    template <class T, class U, class V, class W> Term name(T&& a, U&& b, V&& c, W&& d, OptArgs&& optarg = {}) && { \
        return Term(TT::type, std::vector<Term>{ std::move(*this),    \
        wrap(expr(std::forward<T>(a))), wrap(expr(std::forward<U>(b))), \
        wrap(expr(std::forward<V>(c))), wrap(expr(std::forward<W>(d))) }, std::move(optarg)); } \
    template <class T, class U, class V, class W> Term name(T&& a, U&& b, V&& c, W&& d, OptArgs&& optarg = {}) const & { \
        return Term(TT::type, std::vector<Term>{ *this,        \
        wrap(expr(std::forward<T>(a))), wrap(expr(std::forward<U>(b))), \
        wrap(expr(std::forward<V>(c))), wrap(expr(std::forward<W>(d))) }, std::move(optarg)); }
#define CO_(name, type, wrap)                                       \
    C_(name, type, wrap)                                            \
    CO0(name, type)                                                 \
    CO1(name, type, wrap)                                           \
    CO2(name, type, wrap)                                           \
    CO3(name, type, wrap)                                           \
    CO4(name, type, wrap)
#define no_wrap(x) x

    CO1(table_create, TABLE_CREATE, no_wrap)
    C1(table_drop, TABLE_DROP, no_wrap)
    C0(table_list, TABLE_LIST)
    CO1(index_create, INDEX_CREATE, no_wrap)
    CO2(index_create, INDEX_CREATE, func_wrap)
    C1(index_drop, INDEX_DROP, no_wrap)
    C0(index_list, INDEX_LIST)
    CO2(index_rename, INDEX_RENAME, no_wrap)
    C_(index_status, INDEX_STATUS, no_wrap)
    C_(index_wait, INDEX_WAIT, no_wrap)
    CO0(changes, CHANGES)
    CO1(insert, INSERT, no_wrap)
    CO1(update, UPDATE, func_wrap)
    CO1(replace, REPLACE, func_wrap)
    CO0(delete_, DELETE)
    C0(sync, SYNC)
    CO1(table, TABLE, no_wrap)
    C1(get, GET, no_wrap)
    CO_(get_all, GET_ALL, no_wrap)
    CO2(between, BETWEEN, no_wrap)
    CO1(filter, FILTER, func_wrap)
    C2(inner_join, INNER_JOIN)
    C2(outer_join, OUTER_JOIN)
    CO2(eq_join, EQ_JOIN, func_wrap)
    C0(zip, ZIP)
    C_(map, MAP, func_wrap)
    C_(with_fields, WITH_FIELDS, no_wrap)
    C1(concat_map, CONCAT_MAP, func_wrap)
    CO_(order_by, ORDER_BY, func_wrap)
    C1(skip, SKIP, no_wrap)
    C1(limit, LIMIT, no_wrap)
    CO1(slice, SLICE, no_wrap)
    CO2(slice, SLICE, no_wrap)
    C1(nth, NTH, no_wrap)
    C1(offsets_of, OFFSETS_OF, func_wrap)
    C0(is_empty, IS_EMPTY)
    CO_(union_, UNION, no_wrap)
    C1(sample, SAMPLE, no_wrap)
    CO_(group, GROUP, func_wrap)
    C0(ungroup, UNGROUP)
    C1(reduce, REDUCE, no_wrap)
    CO2(fold, FOLD, no_wrap)
    C0(count, COUNT)
    C1(count, COUNT, func_wrap)
    C0(sum, SUM)
    C1(sum, SUM, func_wrap)
    C0(avg, AVG)
    C1(avg, AVG, func_wrap)
    C1(min, MIN, func_wrap)
    CO0(min, MIN)
    C1(max, MAX, func_wrap)
    CO0(max, MAX)
    CO0(distinct, DISTINCT)
    C_(contains, CONTAINS, func_wrap)
    C_(pluck, PLUCK, no_wrap)
    C_(without, WITHOUT, no_wrap)
    C_(merge, MERGE, func_wrap)
    C1(append, APPEND, no_wrap)
    C1(prepend, PREPEND, no_wrap)
    C1(difference, DIFFERENCE, no_wrap)
    C1(set_insert, SET_INSERT, no_wrap)
    C1(set_union, SET_UNION, no_wrap)
    C1(set_intersection, SET_INTERSECTION, no_wrap)
    C1(set_difference, SET_DIFFERENCE, no_wrap)
    C1(operator[], BRACKET, no_wrap)
    C1(get_field, GET_FIELD, no_wrap)
    C_(has_fields, HAS_FIELDS, no_wrap)
    C2(insert_at, INSERT_AT)
    C2(splice_at, SPLICE_AT)
    C1(delete_at, DELETE_AT, no_wrap)
    C2(delete_at, DELETE_AT)
    C2(change_at, CHANGE_AT)
    C0(keys, KEYS)
    C1(match, MATCH, no_wrap)
    C0(split, SPLIT)
    C1(split, SPLIT, no_wrap)
    C2(split, SPLIT)
    C0(upcase, UPCASE)
    C0(downcase, DOWNCASE)
    C_(add, ADD, no_wrap)
    C1(operator+, ADD, no_wrap)
    C_(sub, SUB, no_wrap)
    C1(operator-, SUB, no_wrap)
    C_(mul, MUL, no_wrap)
    C1(operator*, MUL, no_wrap)
    C_(div, DIV, no_wrap)
    C1(operator/, DIV, no_wrap)
    C1(mod, MOD, no_wrap)
    C1(operator%, MOD, no_wrap)
    C_(and_, AND, no_wrap)
    C1(operator&&, AND, no_wrap)
    C_(or_, OR, no_wrap)
    C1(operator||, OR, no_wrap)
    C1(eq, EQ, no_wrap)
    C1(operator==, EQ, no_wrap)
    C1(ne, NE, no_wrap)
    C1(operator!=, NE, no_wrap)
    C1(gt, GT, no_wrap)
    C1(operator>, GT, no_wrap)
    C1(ge, GE, no_wrap)
    C1(operator>=, GE, no_wrap)
    C1(lt, LT, no_wrap)
    C1(operator<, LT, no_wrap)
    C1(le, LE, no_wrap)
    C1(operator<=, LE, no_wrap)
    C0(not_, NOT)
    C0(operator!, NOT)
    C1(in_timezone, IN_TIMEZONE, no_wrap)
    C0(timezone, TIMEZONE)
    CO2(during, DURING, no_wrap)
    C0(date, DATE)
    C0(time_of_day, TIME_OF_DAY)
    C0(year, YEAR)
    C0(month, MONTH)
    C0(day, DAY)
    C0(day_of_week, DAY_OF_WEEK)
    C0(day_of_year, DAY_OF_YEAR)
    C0(hours, HOURS)
    C0(minutes, MINUTES)
    C0(seconds, SECONDS)
    C0(to_iso8601, TO_ISO8601)
    C0(to_epoch_time, TO_EPOCH_TIME)
    C1(for_each, FOR_EACH, func_wrap)
    C1(default_, DEFAULT, no_wrap)
    CO1(js, JAVASCRIPT, no_wrap)
    C1(coerce_to, COERCE_TO, no_wrap)
    C0(type_of, TYPE_OF)
    C0(info, INFO)
    C0(to_json, TO_JSON_STRING)
    C0(to_json_string, TO_JSON_STRING)
    C1(distance, DISTANCE, no_wrap)
    C0(fill, FILL)
    C0(to_geojson, TO_GEOJSON)
    CO1(get_intersecting, GET_INTERSECTING, no_wrap)
    CO1(get_nearest, GET_NEAREST, no_wrap)
    C1(includes, INCLUDES, no_wrap)
    C1(intersects, INTERSECTS, no_wrap)
    C1(polygon_sub, POLYGON_SUB, no_wrap)
    C0(config, CONFIG)
    C0(rebalance, REBALANCE)
    CO0(reconfigure, RECONFIGURE)
    C0(status, STATUS)
    CO0(wait, WAIT)
    C0(floor, FLOOR)
    C0(ceil, CEIL)
    C0(round, ROUND)
    C0(values, VALUES)

    // The expansion of this macro fails to compile on some versions of GCC and Clang:
    // C_(operator(), FUNCALL, no_wrap)
    // The std::enable_if makes the error go away

    // $doc(do)

    template <class T, class ...U>
    typename std::enable_if<!std::is_same<T, Var>::value, Term>::type
    operator() (T&& a, U&& ...b) && {
        return Term(TT::FUNCALL, std::vector<Term>{
                std::move(*this),
                expr(std::forward<T>(a)),
                expr(std::forward<U>(b))... });
    }
    template <class T, class ...U>
    typename std::enable_if<!std::is_same<T, Var>::value, Term>::type
    operator() (T&& a, U&& ...b) const & {
        return Term(TT::FUNCALL, std::vector<Term>{
                *this,
                expr(std::forward<T>(a)),
                expr(std::forward<U>(b))... });
    }

#undef C0
#undef C1
#undef C2
#undef C_
#undef CO0
#undef CO1
#undef CO2

    // Send the term to the server and return the results.
    // Errors returned by the server are thrown.
    Cursor run(Connection&, OptArgs&& args = {});

    // $doc(do)
    template <class ...T>
    Term do_(T&& ...a) && {
        auto list = { std::move(*this), Term::func_wrap(expr(std::forward<T>(a)))... };
        std::vector<Term> args;
        args.reserve(list.size() + 1);
        args.emplace_back(func_wrap(std::move(*(list.end()-1))));
        for (auto it = list.begin(); it + 1 != list.end(); ++it) {
            args.emplace_back(std::move(*it));
        }
        return Term(TT::FUNCALL, std::move(args));
    }

    // Adds optargs to an already built term
    Term opt(OptArgs&& optargs) && {
        return Term(std::move(*this), std::move(optargs));
    }

    // Used internally to implement object()
    static Term make_object(std::vector<Term>&&);

    // Used internally to implement array()
    static Term make_binary(Term&&);

    Datum get_datum() const;

private:
    friend class Var;
    friend class Connection;
    friend struct Query;

    template <int _>
    Var mkvar(std::vector<int>& vars);

    template <class F, int ...N>
    void set_function(F);

    Datum alpha_rename(Term&&);

    Term(Term&& orig, OptArgs&& optargs);

    std::map<int, int*> free_vars;
    Datum datum;
};

// A term representing null
Term nil();

template <class T>
Term expr(T&& a) {
    return Term(std::forward<T>(a));
}

// Represents a ReQL variable.
// This type is passed to functions used in ReQL queries.
class Var {
public:
    // Convert to a term
    Term operator*() const {
        Term term(TT::VAR, std::vector<Term>{expr(*id)});
        term.free_vars = {{*id, id}};
        return term;
    }

    Var(int* id_) : id(id_) { }
private:
    int* id;
};

template <int N>
Var Term::mkvar(std::vector<int>& vars) {
    int id = gen_var_id();
    vars.push_back(id);
    return Var(&*vars.rbegin());
}

template <class F, int ...N>
void Term::set_function(F f) {
    std::vector<int> vars;
    vars.reserve(sizeof...(N));
    std::vector<Var> args = { mkvar<N>(vars)... };
    Term body = f(args[N] ...);

    int* low = &*vars.begin();
    int* high = &*(vars.end() - 1);
    for (auto it = body.free_vars.begin(); it != body.free_vars.end(); ) {
        if (it->second >= low && it->second <= high) {
            if (it->first != *it->second) {
                throw Error("Internal error: variable index mis-match");
            }
            ++it;
        } else {
            free_vars.emplace(*it);
            ++it;
        }
    }
    datum = Array{TT::FUNC, Array{Array{TT::MAKE_ARRAY, vars}, body.datum}};
}

// These macros are similar to those defined above, but for top-level ReQL operations

#define C0(name) Term name();
#define C0_IMPL(name, type) Term name() { return Term(TT::type, std::vector<Term>{}); }
#define CO0(name) Term name(OptArgs&& optargs = {});
#define CO0_IMPL(name, type) Term name(OptArgs&& optargs) { return Term(TT::type, std::vector<Term>{}, std::move(optargs)); }
#define C1(name, type, wrap) template <class T> Term name(T&& a) {          \
        return Term(TT::type, std::vector<Term>{ wrap(expr(std::forward<T>(a))) }); }
#define C2(name, type) template <class T, class U> Term name(T&& a, U&& b) { \
        return Term(TT::type, std::vector<Term>{ expr(std::forward<T>(a)), expr(std::forward<U>(b)) }); }
#define C3(name, type) template <class A, class B, class C>    \
    Term name(A&& a, B&& b, C&& c) { return Term(TT::type, std::vector<Term>{ \
                expr(std::forward<A>(a)), expr(std::forward<B>(b)), expr(std::forward<C>(c)) }); }
#define C4(name, type) template <class A, class B, class C, class D>    \
    Term name(A&& a, B&& b, C&& c, D&& d) { return Term(TT::type, std::vector<Term>{ \
                expr(std::forward<A>(a)), expr(std::forward<B>(b)),     \
                    expr(std::forward<C>(c)), expr(std::forward<D>(d))}); }
#define C7(name, type) template <class A, class B, class C, class D, class E, class F, class G> \
    Term name(A&& a, B&& b, C&& c, D&& d, E&& e, F&& f, G&& g) { return Term(TT::type, std::vector<Term>{ \
        expr(std::forward<A>(a)), expr(std::forward<B>(b)), expr(std::forward<C>(c)), \
        expr(std::forward<D>(d)), expr(std::forward<E>(e)), expr(std::forward<F>(f)), \
            expr(std::forward<G>(g))}); }
#define C_(name, type, wrap) template <class ...T> Term name(T&& ...a) {    \
        return Term(TT::type, std::vector<Term>{ wrap(expr(std::forward<T>(a)))... }); }
#define CO1(name, type, wrap) template <class T> Term name(T&& a, OptArgs&& optarg = {}) {       \
        return Term(TT::type, std::vector<Term>{ wrap(expr(std::forward<T>(a)))}, std::move(optarg)); }
#define CO2(name, type) template <class T, class U> Term name(T&& a, U&& b, OptArgs&& optarg = {}) { \
        return Term(TT::type, std::vector<Term>{ expr(std::forward<T>(a)), expr(std::forward<U>(b))}, std::move(optarg)); }
#define func_wrap Term::func_wrap

C1(db_create, DB_CREATE, no_wrap)
C1(db_drop, DB_DROP, no_wrap)
C0(db_list)
CO1(table_create, TABLE_CREATE, no_wrap)
C1(table_drop, TABLE_DROP, no_wrap)
C0(table_list)
C1(db, DB, no_wrap)
CO1(table, TABLE, no_wrap)
C_(add, ADD, no_wrap)
C2(sub, SUB)
C_(mul, MUL, no_wrap)
C_(div, DIV, no_wrap)
C2(mod, MOD)
C_(and_, AND, no_wrap)
C_(or_, OR, no_wrap)
C2(eq, EQ)
C2(ne, NE)
C2(gt, GT)
C2(ge, GE)
C2(lt, LT)
C2(le, LE)
C1(not_, NOT, no_wrap)
CO0(random)
CO1(random, RANDOM, no_wrap)
CO2(random, RANDOM)
C0(now)
C4(time, TIME)
C7(time, TIME)
C1(epoch_time, EPOCH_TIME, no_wrap)
CO1(iso8601, ISO8601, no_wrap)
CO1(js, JAVASCRIPT, no_wrap)
C1(args, ARGS, no_wrap)
C_(branch, BRANCH, no_wrap)
C0(range)
C1(range, RANGE, no_wrap)
C2(range, RANGE)
C0(error)
C1(error, ERROR, no_wrap)
C1(json, JSON, no_wrap)
CO1(http, HTTP, func_wrap)
C0(uuid)
C1(uuid, UUID, no_wrap)
CO2(circle, CIRCLE)
C1(geojson, GEOJSON, no_wrap)
C_(line, LINE, no_wrap)
C2(point, POINT)
C_(polygon, POLYGON, no_wrap)
C_(array, MAKE_ARRAY, no_wrap)
C1(desc, DESC, func_wrap)
C1(asc, ASC, func_wrap)
C0(literal)
C1(literal, LITERAL, no_wrap)
C1(type_of, TYPE_OF, no_wrap)
C_(map, MAP, func_wrap)
C1(floor, FLOOR, no_wrap)
C1(ceil, CEIL, no_wrap)
C1(round, ROUND, no_wrap)
C_(union_, UNION, no_wrap)
C_(group, GROUP, func_wrap)
C1(count, COUNT, no_wrap)
C_(count, COUNT, func_wrap)
C1(sum, SUM, no_wrap)
C_(sum, SUM, func_wrap)
C1(avg, AVG, no_wrap)
C_(avg, AVG, func_wrap)
C1(min, MIN, no_wrap)
C_(min, MIN, func_wrap)
C1(max, MAX, no_wrap)
C_(max, MAX, func_wrap)
C1(distinct, DISTINCT, no_wrap)
C1(contains, CONTAINS, no_wrap)
C_(contains, CONTAINS, func_wrap)

#undef C0
#undef C1
#undef C2
#undef C3
#undef C4
#undef C7
#undef C_
#undef CO1
#undef CO2
#undef func_wrap

// $doc(do)
template <class R, class ...T>
Term do_(R&& a, T&& ...b) {
    return expr(std::forward<R>(a)).do_(std::forward<T>(b)...);
}

// $doc(object)
template <class ...T>
Term object(T&& ...a) {
    return Term::make_object(std::vector<Term>{ expr(std::forward<T>(a))... });
}

// $doc(binary)
template <class T>
Term binary(T&& a) {
    return Term::make_binary(expr(std::forward<T>(a)));
}

// Construct an empty optarg
OptArgs optargs();

// Construct an optarg made out of pairs of arguments
// For example: optargs("k1", v1, "k2", v2)
template <class V, class ...T>
OptArgs optargs(const char* key, V&& val, T&& ...rest) {
    OptArgs opts = optargs(rest...);
    opts.emplace(key, expr(std::forward<V>(val)));
    return opts;
}

extern Term row;
extern Term maxval;
extern Term minval;
extern Term january;
extern Term february;
extern Term march;
extern Term april;
extern Term may;
extern Term june;
extern Term july;
extern Term august;
extern Term september;
extern Term october;
extern Term november;
extern Term december;
extern Term monday;
extern Term tuesday;
extern Term wednesday;
extern Term thursday;
extern Term friday;
extern Term saturday;
extern Term sunday;
}
