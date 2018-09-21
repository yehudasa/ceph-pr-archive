add_library(rados_unleashed_objs OBJECT
  RADOSImpl.cc)
add_library(rados_unleashed_api_obj OBJECT
  rados_unleashed.cc)

if(ENABLE_SHARED)
  add_library(librados_unleashed ${CEPH_SHARED}
    $<TARGET_OBJECTS:rados_unleashed_api_obj>
    $<TARGET_OBJECTS:rados_unleashed_objs>
    $<TARGET_OBJECTS:common_buffer_obj>)
  set_target_properties(librados_unleashed PROPERTIES
    OUTPUT_NAME rados_unleashed
    VERSION 2.0.0
    SOVERSION 2
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  if(NOT APPLE)
    set_property(TARGET librados_unleashed APPEND_STRING PROPERTY
      LINK_FLAGS " -Wl,--exclude-libs,ALL")
  endif()
else(ENABLE_SHARED)
  add_library(librados_unleashed STATIC
    $<TARGET_OBJECTS:rados_unleashed_api_obj>
    $<TARGET_OBJECTS:rados_unleashed_objs>)
endif(ENABLE_SHARED)
target_link_libraries(librados_unleashed PRIVATE
  osdc ceph-common cls_lock_client
  ${BLKID_LIBRARIES} ${CRYPTO_LIBS} ${EXTRALIBS})
target_link_libraries(librados_unleashed ${rados_libs})
install(TARGETS librados_unleashed DESTINATION ${CMAKE_INSTALL_LIBDIR})
