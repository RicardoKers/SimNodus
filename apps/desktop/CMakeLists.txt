find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets)
add_executable(simnodus_document_editor main.cpp document_window.cpp preview_canvas.cpp)
target_link_libraries(simnodus_document_editor PRIVATE
    simnodus_editor_document simnodus_symbol_preview Qt6::Core Qt6::Gui Qt6::Widgets)
if(MSVC)
    target_compile_options(simnodus_document_editor PRIVATE /W4 /WX /permissive- /EHsc)
endif()
