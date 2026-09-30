find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
add_executable(simnodus_document_editor main.cpp document_window.cpp)
target_link_libraries(simnodus_document_editor PRIVATE
    simnodus_editor_document Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
    target_compile_options(simnodus_document_editor PRIVATE /W4 /WX /permissive- /EHsc)
endif()
