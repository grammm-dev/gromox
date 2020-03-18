#!/bin/sh

chmod a+x library/scripts/*
chmod a+x mta-system/scripts/*
chmod a+x mra-system/scripts/*
chmod a+x core-system/scripts/*
chmod a+x agent-system/scripts/*
chmod a+x DOMAIN_ADMIN/scripts/*
chmod a+x SYSTEM_ADMIN/scripts/*
chmod a+x ARCHIVE_SYSTEM/scripts/*
cd library && make && cd ..
cd mta-system
ln -s ../library/common .
ln -s ../library/email_lib .
ln -s ../library/epoll_scheduler .
ln -s ../library/mapi_lib .
ln -s ../library/rpc_lib .
make && make release && cd ..
cd mra-system
ln -s ../library/common .
ln -s ../library/email_lib .
ln -s ../library/epoll_scheduler .
ln -s ../library/mapi_lib .
ln -s ../library/rpc_lib .
make && make release && cd ..
cd core-system
ln -s ../library/common .
ln -s ../library/email_lib .
ln -s ../library/epoll_scheduler .
ln -s ../library/mapi_lib .
ln -s ../library/rpc_lib .
ln -s ../library/webkit_lib .
make && make release && cd ..
cd agent-system
ln -s ../library/common .
ln -s ../library/email_lib .
ln -s ../library/mapi_lib .
ln -s ../library/rpc_lib .
ln -s ../library/webkit_lib .
make && make release && cd ..
cd DOMAIN_ADMIN
ln -s ../library/common .
ln -s ../library/email_lib .
ln -s ../library/mapi_lib .
ln -s ../library/rpc_lib .
ln -s ../library/webkit_lib .
make && make release && cd ..
cd SYSTEM_ADMIN
ln -s ../library/common .
ln -s ../library/email_lib .
ln -s ../library/mapi_lib .
ln -s ../library/webkit_lib .
make && make release && cd ..
cd ARCHIVE_SYSTEM
ln -s ../library/common .
ln -s ../library/email_lib .
make && make release && cd ..
