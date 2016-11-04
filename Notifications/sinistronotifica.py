import time
import pycurl, json
import sys

# tempo entre notificoes em segundo

DELAY = 20

# InstaPush API setup
appID = "580b6afca4c48a4b693a9426"
appSecret = "4934d1e32b7eff991c6b96b94a6aa1b3"
pushEvent = "Acidente_"
pushMessage = "Acidente a frente"


# use Curl to post to the Instapush API
c = pycurl.Curl()

# set API URL
c.setopt(c.URL, 'https://api.instapush.im/v1/post')

#setup custom headers for authentication variables and content type
c.setopt(c.HTTPHEADER, ['x-instapush-appid: ' + appID,
			'x-instapush-appsecret: ' + appSecret,
			'Content-Type: application/json'])


# create a dict structure for the JSON data to post
json_fields = {}

# setup JSON values
json_fields['event']=pushEvent
json_fields['trackers'] = {}
json_fields['trackers']['message']=pushMessage
#print(json_fields)
postfields = json.dumps(json_fields)

# make sure to send the JSON with post
c.setopt(c.POSTFIELDS, postfields)

# set this so we can capture the resposne in our buffer
#c.setopt(c.WRITEFUNCTION, buffer.write)

# uncomment to see the post sent
#c.setopt(c.VERBOSE, True)

try:
	# Loop infinito que envia push notificantions a cada 20 segundos 
		c.perform()
		#print(pushMessage)
		# tempo de espera para envio de nova mensagem
		c.close()
		sys.exit()
except KeyboardInterrupt:
        pass
