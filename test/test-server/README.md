# Nginx test server

This contains docker-compose and nginx configuration for setting up a small test server which can be used to check
that POSTs that the light pet is sending are valid html and that ssl connections are working and certs can be
validated against a root cert.

## Usage

Modify the `nginx.conf` to include the hostname/ip of your server in the relevant locations, and place certs and keys
in a `ssl` subfolder and refer to these in the `nginx.conf`. Then use `docker-compose up` to start up the server.
For more advanced usage, see the docker docs.

To debug, uncomment the line near to the top of the `nginx.conf` which will enable debugging output.

You may need to do some iptables fiddling in order to route requests to your network external IP to the localhost IP
that the container binds to. I used something like:
```
sudo iptables -t nat -A OUTPUT -p tcp --dport 443 -d <your local ip> -j DNAT --to-destination 127.0.0.1:443
```

