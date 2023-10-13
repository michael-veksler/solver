# solver
A fast generic [Constraint Solver](https://en.wikipedia.org/wiki/Constraint_satisfaction_problem). 

[![ci](https://github.com/michael-veksler/solver/actions/workflows/ci.yml/badge.svg)](https://github.com/michael-veksler/solver/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/michael-veksler/solver/branch/main/graph/badge.svg)](https://codecov.io/gh/michael-veksler/solver)
[![CodeQL](https://github.com/michael-veksler/solver/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/michael-veksler/solver/actions/workflows/codeql-analysis.yml)

## About solver

Goals and technology:
 1. Has a modular structure, that facilitates seamless integration with other solvers. 
    The types of solvers that are compatible: 
    * A [DPLL](https://en.wikipedia.org/wiki/DPLL_algorithm)/[CDCL](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning)
      [SAT solver](https://en.wikipedia.org/wiki/Boolean_satisfiability_problem) which has an interface to:
      * Run only [BCP](https://en.wikipedia.org/wiki/Unit_propagation).
      * Has an efficient query interface to find all literals that the last BCP has inferred.
      * Has an interface that allows the integration with external
        [CDCL](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning).
    * A Multi-Valued SAT solver, with similar capabilities to the Boolean-SAT described above.
    * A Constraint Solver based on [AC-3](https://en.wikipedia.org/wiki/AC-3_algorithm) 
      or Global AC-3 (GAC-3) which has an interface to:
      * Run only the [Constraint Propagation](https://en.wikipedia.org/wiki/Local_consistency#Constraint_propagation_for_arc_and_path_consistency) phase.
      * Has an efficient query interface to find all literals that the last Constraint Propagation has inferred.
      * Has an interface that allows the integration with external
        [CDCL](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning). 
        This means either that the solver has CDCL by itself, like [HaifaCSP](https://strichman.net.technion.ac.il/haifacsp/), or has a way to trace-back the history of propagation.
    * **Maybe**: a [Local Search Solver](https://en.wikipedia.org/wiki/Local_search_(constraint_satisfaction)) -
      to be used for decision making. 
 2. Employ [CDCL](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning) over the whole problem,
    including generic constraint learning, similar to HaifaCSP](https://strichman.net.technion.ac.il/haifacsp/).
 3. Support a mix of different types of propagation and consistency, where Global AC-3 is the strongest
    and bounds-consistency is the weakest.
 4. Have an extendable variable-domain. Examples of domains:
    * Boolean variables.
    * Tiny integer-domains, sets of which will be implemented as bit-vectors.
    * 32-bit integer-domains implemented as collections of intervals, such as [interval_set](https://www.boost.org/doc/libs/1_59_0/libs/icl/doc/html/boost/icl/interval_set.html).
    * Bounds, either of integers (fixed or [unlimited precision](https://www.boost.org/doc/libs/1_80_0/libs/multiprecision/doc/html/boost_multiprecision/tut/ints/gmp_int.html)), or floating-point.
 5. Different solvers work with different types of domains. 
    Some of the solvers will have a single domain type, such as SAT-solvers, 
    which typically work only with Boolean domains. 
    This use of single-domain types allows an efficient static-dispatch. 
    This is more efficient than using virtual functions to get to the correct functions at run-time.
 6. Synching between different domain-types is performed by a special equality solver that can access all domain types.


 


## More Details

 * [Dependency Setup](README_dependencies.md)
 * [Building Details](README_building.md)
 * [Troubleshooting](README_troubleshooting.md)
 * [Docker](README_docker.md)
