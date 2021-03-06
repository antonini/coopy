
option(DOXYGEN_TRICKS "Process *.paradox files for documentation" FALSE)
option(GENERATE_PDF "Generate *.pdf files for documentation" FALSE)
option(GENERATE_MAN_PAGES "Generate man pages" FALSE)

if (DOXYGEN_TRICKS)
  file(GLOB paradox ${CMAKE_SOURCE_DIR}/doc/*.paradox)
  set(PARADOXES)
  foreach(f ${paradox})
    get_filename_component(pbase ${f} NAME_WE)
    # message(STATUS "${f} ${pbase}")
    set(ODIR ${CMAKE_SOURCE_DIR}/doc)
    set(ONAME ${pbase}.dox)
    set(GEN_PDF htmlonly)
    if (GENERATE_PDF)
      set(GEN_PDF pdf)
    endif ()
    add_custom_command(OUTPUT ${ODIR}/${ONAME}
      COMMAND mkdir -p ${ODIR}
      COMMAND ${CMAKE_SOURCE_DIR}/scripts/process_dox.sh ${f} ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${GEN_PDF} > ${ODIR}/${ONAME}
      MAIN_DEPENDENCY ${f}
      DEPENDS ${CMAKE_SOURCE_DIR}/scripts/process_dox.sh ss2html ssdiff
      ssresolve ssrediff ssformat sspatch ssmerge coopy
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      )
    set(PARADOXES ${PARADOXES} ${ODIR}/${ONAME})
  endforeach()
endif ()

find_program(DOXYGEN_EXE NAMES doxygen)
mark_as_advanced(DOXYGEN_EXE)

set(MAN_PAGES)
if (GENERATE_MAN_PAGES)
  file(GLOB cmd ${CMAKE_SOURCE_DIR}/doc/cmd_*.paradox)
  foreach(f ${cmd})
    get_filename_component(pbase ${f} NAME_WE)
    string(REPLACE "cmd_" "" pbase ${pbase})
    # message(STATUS "CMD ${pbase}")
    set(page ${CMAKE_BINARY_DIR}/gendoc/man/man3/${pbase}.3)
    install(FILES ${page} COMPONENT documentation DESTINATION man/man3)
    list(APPEND MAN_PAGES ${page})
  endforeach()
endif ()

if (DOXYGEN_EXE)
  foreach (mode html latex man)
    string(TOUPPER ${mode} mode_upper)
    set(ENABLED_SECTIONS)
    set(INPUT "${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/doc")
    set(RECURSIVE YES)
    set(EXCLUDE)
    set(GENERATE_LATEX NO)
    set(GENERATE_HTML NO)
    set(GENERATE_MAN NO)
    set(FILE_PATTERNS "*.cpp *.h *.c *.dox")
    set(GENERATE_${mode_upper} YES)
    if (GENERATE_HTML)
      set(ENABLED_SECTIONS "link_code link_examples link_internal")
    endif ()
    if (GENERATE_LATEX)
      include(CoopyDocLatex)
      set(INPUT ${CMAKE_SOURCE_DIR}/doc/images)
      foreach (i ${LATEX_MANUAL_DOCS})
	set(INPUT "${INPUT} ${CMAKE_SOURCE_DIR}/doc/${i}")
      endforeach()
      set(RECURSIVE YES)
      set(FILE_PATTERNS "*.dox")
      set(ENABLED_SECTIONS "link_internal")
      set(EXCLUDE)
    endif ()
    if (GENERATE_MAN)
      set(INPUT "${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/doc")
      set(RECURSIVE YES)
      set(FILE_PATTERNS "cmd_*.dox")
      set(ENABLED_SECTIONS "link_internal MAN_PAGE_COND")
    endif ()
    set(DOXYGEN_TEMPLATE_DIR "" CACHE STRING "Directory holding doxygen styling, if desired")
    if (DOXYGEN_TEMPLATE_DIR)
      set(HTML_HEADER ${DOXYGEN_TEMPLATE_DIR}/header.html)
      set(HTML_FOOTER ${DOXYGEN_TEMPLATE_DIR}/footer.html)
      set(HTML_STYLESHEET ${DOXYGEN_TEMPLATE_DIR}/style.css)
    endif ()
    configure_file(${CMAKE_SOURCE_DIR}/conf/coopy_doxygen.conf.in
      ${CMAKE_BINARY_DIR}/coopy_doxygen_${mode}.conf IMMEDIATE)

    make_directory(${CMAKE_BINARY_DIR}/layout/${mode})

    if (GENERATE_MAN AND GENERATE_MAN_PAGES)
      add_custom_command(OUTPUT ${MAN_PAGES}
	COMMAND ${DOXYGEN_EXE} ${CMAKE_BINARY_DIR}/coopy_doxygen_${mode}.conf
	DEPENDS ${PARADOXES})
      add_custom_target(${mode} ALL SOURCES ${MAN_PAGES})
    else ()
      add_custom_target(${mode} 
	COMMAND ${DOXYGEN_EXE} ${CMAKE_BINARY_DIR}/coopy_doxygen_${mode}.conf
	DEPENDS ${PARADOXES})
    endif ()
  endforeach ()
endif ()

if (DOXYGEN_TRICKS)
  if (GENERATE_PDF)
    add_custom_target(xpdf 
      COMMAND ${CMAKE_SOURCE_DIR}/scripts/make_pdf.sh)
    add_custom_target(guide 
      ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/gendoc/latex/refman.pdf ${CMAKE_SOURCE_DIR}/CoopyGuide.pdf
      DEPENDS xpdf
      ${CMAKE_SOURCE_DIR}/scripts/fix_images.sh
      ${CMAKE_SOURCE_DIR}/scripts/fix_order.sh
      )
  endif()
endif ()

configure_file(${CMAKE_SOURCE_DIR}/doc/tdiff/tdiff_spec_draft.html
  ${CMAKE_BINARY_DIR}/gendoc/html/tdiff_spec_draft.html IMMEDIATE)
