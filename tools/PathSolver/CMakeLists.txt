if (DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support Svf Cudd)
    add_llvm_tool(path-solver main.cpp)
else ()
    if (LLVM_LINK_LLVM_DYLIB)
        set(llvm_libs LLVM)
    else ()
        llvm_map_components_to_libnames(llvm_libs BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support TransformUtils)
    endif ()
    add_executable(path-solver main.cpp)

    target_link_libraries(path-solver Svf Cudd ${llvm_libs})

    set_target_properties(path-solver PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif ()
