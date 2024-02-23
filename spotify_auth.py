from flask import Flask, request, redirect
import requests
import base64

app = Flask(__name__)
CLIENT_ID = "***REMOVED***"
CLIENT_SECRET = "***REMOVED***"
REDIRECT_URI = "http://localhost:5555/callback"

@app.route("/")
def authorize():
    URL = "https://accounts.spotify.com/authorize?"
    URL += "response_type=code&"
    URL += "client_id=" + CLIENT_ID + "&"
    URL += "scope=user-read-currently-playing,user-read-playback-state&"
    URL += "redirect_uri=" + REDIRECT_URI
    return redirect(URL, code=302)


@app.route("/callback")
def callback():
    code = request.args.get("code")
    r = requests.post("https://accounts.spotify.com/api/token", data={
        "grant_type" : "authorization_code",
        "redirect_uri" : REDIRECT_URI,
        "code" : code
    }, headers={
        "Authorization" : "Basic " + base64.b64encode("{}:{}".format(CLIENT_ID, CLIENT_SECRET).encode()).decode(),
        "Content-type" : "application/x-www-form-urlencoded"
    })
    return r.json()

@app.route("/refresh")
def refresh():
    refresh_token = request.args.get("refresh_token")
    r = requests.post("https://accounts.spotify.com/api/token", data={
        "grant_type" : "refresh_token",
        "refresh_token" : refresh_token
    }, headers={
        "Authorization" : "Basic " + base64.b64encode("{}:{}".format(CLIENT_ID, CLIENT_SECRET).encode()).decode(),
        "content-type" : "application/x-www-form-urlencoded"
    }, )
    return r.json()

if __name__ == '__main__':
    app.debug=True
    app.run(port=5555)