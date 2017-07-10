
TYPE="Content-Type:application/json"
csr_req_url="https://devregmaster.imw.motorolasolutions.com/csrreq/95ac7dd1-9320-4766-a78f-7d64c35746fa"

curl -X POST -v -d @csr_req.json $csr_req_url --header $TYPE

status=$?

echo $status
