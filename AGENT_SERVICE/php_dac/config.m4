PHP_ARG_ENABLE(dac, whether to enable php-dac support,
[ --enable-dac   Enable php-dac support])

if test "$PHP_DAC" = "yes"; then
	AC_DEFINE(HAVE_DAC, 1, [Whether you have php-dac])
	PHP_NEW_EXTENSION(dac, dac.c type_conversion.c ext_pack.c rpc_ext.c adac_client.c, $ext_shared)
fi
