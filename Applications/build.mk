$(call find-makefile)

# List of one-source-file applications in util.

util_apps := \
	banner basename bd cal cat chgrp chmod chown cksum cmp cp cut date dd \
	decomp16 df dirname dosread du echo ed factor false fdisk fgrep fsck \
	grep head id init kill ll ln logname ls man mkdir mkfs mkfifo mknod \
	mount mv od pagesize passwd patchcpm printenv prtroot ps pwd rm rmdir \
	sleep ssh sort stty sum su sync tee tail touch tr true umount uniq \
	uue wc which who whoami write xargs yes

# ...and in V7/cmd.

v7_cmd_apps := \
	ac accton at atrun col comm cron crypt dc dd deroff diff3 diff diffh \
	join look makekey mesg newgrp pr ptx rev split su sum test time tsort \
	wall

# ...and in V7/games.

v7_games_apps := \
	arithmetic backgammon fish wump 

# Given an app name in $1 and a path in $2, creates a target-exe module.

make_single_app = \
	$(eval $1-$2-app.srcs := $2/$1.c) \
	$(call build, $1-$2-app, target-exe) \
	$(eval apps += $($1-$2-app.exe))

apps :=
$(foreach app, $(util_apps), $(call make_single_app,$(app),util))
$(foreach app, $(v7_cmd_apps), $(call make_single_app,$(app),v7/cmd))
$(foreach app, $(v7_games_apps), $(call make_single_app,$(app),v7/games))

apps: $(apps)
.PHONY: apps

