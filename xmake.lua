--global define begin
add_rules("mode.debug", "mode.release")

set_languages("c++20")
add_ldflags("-lpthread")
add_cxxflags("-g", "-O0", "-fcoroutines")
--global define end

-- coroutine test begin
target("test_task")
    set_kind("binary")
    
    add_includedirs("include")
    add_includedirs("src/include")

    -- add_files("src/*.cc")
    add_files("test/test_task.cc")


target("test_executor")
    set_kind("binary")
    
    add_includedirs("include")
    add_includedirs("src/include")

    -- add_files("src/*.cc")
    add_files("test/test_executor.cc")

-- coroutine test end


-- coroutine static begin


-- coroutine static end