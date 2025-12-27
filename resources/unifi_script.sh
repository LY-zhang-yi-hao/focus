#!/bin/bash

# Enable or disable logging based on an input argument (0 = no debug, 1 = debug)
# 根据入参开启/关闭日志（0 关闭调试，1 开启调试）
debug=$7  # Pass 1 for debugging, 0 for no debugging / 传 1 开启调试、0 关闭调试

if [[ $debug -eq 1 ]]; then
  # Enable logging / 开启日志输出
  exec > /config/scripts/unifi_traffic_rules.log 2>&1
else
  exec > /dev/null 2>&1  # Disable logging if debug is not enabled / 未启用调试则丢弃输出
fi

unifi_username=$2
unifi_password=$3
unifi_controller=$4
target_rule_id=$5  # The rule ID you're targeting / 需要操作的规则 ID
action=$6  # 'enable' or 'disable' / 支持 enable 或 disable
cookie=$(mktemp)
headers=$(mktemp)

curl_cmd="curl -s -S --cookie ${cookie} --cookie-jar ${cookie} --insecure "

unifi_login() {
    # Log in to the UniFi controller and capture CSRF token from headers / 登录 UniFi 控制器并读取响应头里的 CSRF Token
    if [[ $debug -eq 1 ]]; then echo "Logging in to UniFi Controller..."; fi
    login_response=$(${curl_cmd} -H "Content-Type: application/json" -D ${headers} -d "{\"password\":\"$unifi_password\",\"username\":\"$unifi_username\"}" $unifi_controller/api/auth/login)
    if [[ $debug -eq 1 ]]; then echo "Login Response: $login_response"; fi
    csrf_token=$(awk -v IGNORECASE=1 -v FS=': ' '/^X-CSRF-Token/ {print $2}' "${headers}" | tr -d '\r')
    if [[ $debug -eq 1 ]]; then echo "CSRF Token: $csrf_token"; fi
}

unifi_logout() {
    # Log out from the UniFi controller / 登出 UniFi 控制器
    if [[ $debug -eq 1 ]]; then echo "Logging out of UniFi Controller..."; fi
    logout_response=$(${curl_cmd} $unifi_controller/api/auth/logout)
    if [[ $debug -eq 1 ]]; then echo "Logout Response: $logout_response"; fi
}

get_traffic_rules() {
    # Fetch traffic rules / 获取流量规则列表
    if [[ $debug -eq 1 ]]; then echo "Fetching traffic rules..."; fi
    traffic_rules_response=$(${curl_cmd} "$unifi_controller/proxy/network/v2/api/site/default/trafficrules" -H "Content-Type: application/json" --compressed)
    if [[ $debug -eq 1 ]]; then echo "Traffic Rules Response: $traffic_rules_response"; fi
}

find_and_modify_rule() {
    # Extract the rule that matches the target rule ID / 找出目标规则
    if [[ $debug -eq 1 ]]; then echo "Searching for rule with ID: $target_rule_id"; fi
    rule=$(echo "$traffic_rules_response" | jq --arg rule_id "$target_rule_id" '.[] | select(._id == $rule_id)')

    if [ -z "$rule" ]; then
        echo "No matching rule found with ID: $target_rule_id"
        exit 1
    else
        # Set the action based on 'enable' or 'disable' / 根据 enable/disable 设置布尔值
        if [ "$action" == "enable" ]; then
            new_enabled_value=true
        elif [ "$action" == "disable" ]; then
            new_enabled_value=false
        else
            echo "Error: Action must be 'enable' or 'disable'."
            exit 1
        fi

        # Modify the rule by setting 'enabled' to true or false / 修改规则 enabled 字段
        if [[ $debug -eq 1 ]]; then echo "Found rule: $rule"; fi
        updated_rule=$(echo "$rule" | jq --argjson action "$new_enabled_value" '.enabled = $action')
        
        # Send PUT request to modify the rule, including CSRF token in the headers / 发送 PUT 请求并带上 CSRF 头
        if [[ $debug -eq 1 ]]; then echo "Modifying rule with ID: $target_rule_id"; fi
        
        modify_response=$(${curl_cmd} "$unifi_controller/proxy/network/v2/api/site/default/trafficrules/$target_rule_id" -X PUT \
        -H "Content-Type: application/json" \
        -H "X-CSRF-Token: $csrf_token" \
        -d "$updated_rule" --compressed)
        
        if [[ $debug -eq 1 ]]; then echo "Modify Response: $modify_response"; fi
    fi
}

# Ensure all required parameters are provided / 校验必须的参数是否齐全
if [[ $# < 7 ]]; then
    echo "Error: Must include parameters [fwrule] [username] [password] [UDM address] [rule_id] [enable or disable] [0=debug off, 1=debug on]."
    exit -1
fi

# Log in, get traffic rules, find the rule by ID, modify it, and log out
# 登录、获取规则、按 ID 修改，再登出
unifi_login
get_traffic_rules
find_and_modify_rule
unifi_logout
