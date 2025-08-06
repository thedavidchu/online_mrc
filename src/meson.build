subdir('lib')

# NOTE  This must occur after the lib, since mrc_reuse_stack depends on lib
subdir('mrc')

subdir('ttl')
# NOTE  This must occur after the ttl library, since it uses the Accesses.
subdir('predictor')

# NOTE  This depends on 'lib' and 'mrc'.
subdir('analysis')

# NOTE  This depends on 'lib' and 'mrc'.
subdir('run')

# Depends on 'lib'.
subdir('scripts')