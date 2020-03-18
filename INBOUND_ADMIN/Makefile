#add project parts name here
PARTS_LIST=common \
admin_insulation \
admin_invalid \
admin_log \
admin_main \
athena \
backend_table \
cdner \
daemon \
daily_statistic \
daily_status \
dns_table \
domain_mailbox \
domain_whitelist \
forward_table \
from_replace\
ip_blacklist \
ip_whitelist \
ipdomain_table \
keyword_group \
keyword_statistic \
local_setup\
mensual_statistic \
message_sign \
monitor \
rcpt_blacklist \
redirect_table \
relay_allow \
relay_domains \
relay_table \
status_rpc \
synchronization_rpc \
supervising_list \
supervising_statistic \
supervisor \
synchronization_allow \
system_backup \
system_password \
system_setup \
system_users \
tagging_whitelist \
web_adaptor

SCRIPTS_PATH=./scripts
#object files under "lib" directory, used for linking
REMOTE_LINKS=../common/common.so ../webkit_lib/webkit_lib.so

.PHONY : normal
.PHONY : debug
.PHONY : clean
.PHONY : release

normal :
	@$(SCRIPTS_PATH)/environment lib $(REMOTE_LINKS)
	@echo $(PARTS_LIST) > /tmp/list_tmp
	@$(SCRIPTS_PATH)/compile normal /tmp/list_tmp
debug :
	@$(SCRIPTS_PATH)/environment lib $(REMOTE_LINKS)
	@echo $(PARTS_LIST) > /tmp/list_tmp
	@$(SCRIPTS_PATH)/compile debug /tmp/list_tmp
clean :
	@echo $(PARTS_LIST) > /tmp/list_tmp
	@$(SCRIPTS_PATH)/compile clean /tmp/list_tmp
release :
	-@$(SCRIPTS_PATH)/release
