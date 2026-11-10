if (CDI_ECCODES)
  find_package(eccodes)
  if (eccodes_FOUND)
    message(STATUS "Found ecCodes: TRUE")

    # Some ecCodes installations export absolute paths to libraries of the SDK
    # that was used when ecCodes was built. After an Xcode/CommandLineTools
    # update those files no longer exist, and the generated make rules fail with
    #   No rule to make target '.../MacOSX27.sdk/usr/lib/libm.tbd'
    # Drop such stale absolute entries (on macOS -lm comes from libSystem).
    if (TARGET eccodes)
      foreach (_prop IN ITEMS INTERFACE_LINK_LIBRARIES INTERFACE_LINK_DIRECTORIES)
        get_target_property (_entries eccodes ${_prop})
        if (_entries AND NOT _entries MATCHES "-NOTFOUND$")
          set (_kept "")
          foreach (_entry IN LISTS _entries)
            if (IS_ABSOLUTE "${_entry}" AND NOT EXISTS "${_entry}")
              message(STATUS "ecCodes: dropping missing ${_prop} entry: ${_entry}")
            else ()
              list(APPEND _kept "${_entry}")
            endif ()
          endforeach ()
          set_target_properties(eccodes PROPERTIES ${_prop} "${_kept}")
        endif ()
      endforeach ()
    endif ()

    target_link_libraries(libcdi PUBLIC eccodes)
    set(HAVE_LIBGRIB_API 1)
  else ()
    message(STATUS "eccodes not found: GRIB2 support via eccodes is disabled")
  endif ()
endif ()
