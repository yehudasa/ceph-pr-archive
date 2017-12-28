set(libclient_srcs
  Client.cc
  Dentry.cc
  Fh.cc
  Inode.cc
  MetaRequest.cc
  ClientSnapRealm.cc
  MetaSession.cc
  Trace.cc
  posix_acl.cc
  Delegation.cc)
add_library(client STATIC 
    ${libclient_srcs}
    $<TARGET_OBJECTS:heap_profiler_objs>)
target_link_libraries(client osdc ${ALLOC_LIBS})
