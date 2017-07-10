#!/bin/bash

uploadDir=$(dirname "${BASH_SOURCE[0]}")

hostname=idmmaster.imw.motorolasolutions.com
port=9032
protocol=https
endpoint=/as/authorization.oauth2

#For use in scripts to enable the only output to be the token
quiet=false

function help() {
  echo "Usage: $0 [-e|-env  master|develop] [-h|--hostname hostname] [-p|--port port] [-l|--protocol protocol] [-c cert_file] [-k|--key key_file] [-s|--secret cert_secret] [--quiet]"
  echo
  echo "    --quiet   For use with scripts to ensure the only output is the token"
  exit 1
}

function test_param() {
  key=$1
  value=$2
  if [ -z "$value" ]; then
    echo "Missing argument for the '$key' parameter"
    help
  fi
}

bearerTokenHeaderFile=$inDir/bearer_token_header.json
bearerTokenTemplateFile=$inDir/bearer_token_template.json
encodedTokenFile=$outDir/bearer.token
tokenFile=$outDir/bearer_token.json
agencyId=

while [[ $# > 0  ]]
do
  key="$1"

  case $key in
    -h|--hostname)
    test_param $1 $2
    hostname="$2"
    shift
    shift
    ;;
    -p|--port)
    test_param $1 $2
    port="$2"
    shift
    shift
    ;;
    -l|--protocol)
    test_param $1 $2
    protocol="$2"
    shift
    shift
    ;;
    -c|--cert)
    test_param $1 $2
    certFile="$2"
    shift
    shift
    ;;
    -k|--key)
    test_param $1 $2
    keyFile="$2"
    shift
    shift
    ;;
    -s|--secret)
    test_param $1 $2
    certSecret="$2"
    shift
    shift
    ;;
    --quiet)
    quiet=true
    shift
    ;;
    -e|--env)
    test_param $1 $2
    environment="$2"
    shift
    shift
    case $environment in
       master)
       hostname=idmmaster.imw.motorolasolutions.com
       port=9032
       protocol=https
       ;;
       develop|dev)
       hostname=idmcc.imw.motorolasolutions.com
       port=9032
       protocol=https
       ;;
       release|rel)
       hostname=idmcc.imw.motorolasolutions.com
       port=9032
       protocol=https
       ;;
       production|prod)
       hostname=idm.imw.motorolasolutions.com
       port=9032
       protocol=https
       ;;
       production_ie|prod_ie)
       hostname=idm.imw.motorolasolutions.com
       port=9032
       protocol=https
       ;;
       *)
       echo "Invalid environment: $environment"
       help
       ;;
    esac
    ;;
    *)
    echo "Invalid parameter: $key"
    help
    ;;

  esac
done

if [ -z "$certFile" ]; then
  echo "Missing argument for the certificate parameter"
  help
fi
  
if [ -z "$keyFile" ]; then
  echo "Missing argument for the key parameter"
  help
fi
 
if [ -z "$certSecret" ]; then
  echo "Missing argument for the certificate secret parameter"
  help
fi 
  
address=$protocol://$hostname:$port$endpoint
if [ "$quiet" = false ]; then
   echo "Using $address"
   echo "curl --dump-header header.tmp --cert $certFile:$certSecret --key $keyFile -X POST --data response_type=token --data scope=msi_vault.upload --data client_id=fusion  --data-urlencode redirect_uri=napps://localhost/  $address"
fi 

curl --dump-header header.tmp --cert $certFile:$certSecret --key $keyFile -X POST --data response_type=token --data scope=msi_vault.upload --data client_id=fusion  --data-urlencode redirect_uri=napps://localhost/  $address
LocationHdr=`cat header.tmp |grep -i access_token`
if [ "$quiet" = false ]; then
   echo $LocationHdr
fi 

#Header format is like Location: napps://localhost/#access_token=eyJhbGciOiJSUzM4NCIsImtpZCI6InNpZ25pbmdrZXkiLCJ4NXQiOiJ1eXQ0VExEb3JGbk9UbkNNNU5LZVRPcDFWamMifQ.eyJzdWIiOiJDTj0xMjNCT0I0NTY3LE9VPURldmljZSBBdXRoZW50aWNhdGlvbiBDZXJ0aWZpY2F0ZSxPPW1zaS50ZXN0LEw9Q2hpY2FnbyxTVD1JTCxDPVVTIiwiRE4iOiJDTj0xMjNCT0I0NTY3LE9VPURldmljZSBBdXRoZW50aWNhdGlvbiBDZXJ0aWZpY2F0ZSxPPW1zaS50ZXN0LEw9Q2hpY2FnbyxTVD1JTCxDPVVTIiwiYWdlbmN5IjoibXNpLnRlc3QiLCJkZXZpY2VJZCI6IjEyM0JPQjQ1NjciLCJleHAiOjE0NjIzNDE4MjcsInNjb3BlIjpbIm1zaV92YXVsdC51cGxvYWQiXSwiY2xpZW50X2lkIjoiZnVzaW9uIiwiaXNzIjoiaHR0cHM6Ly9pZG1tYXN0ZXIuaW13Lm1vdG9yb2xhc29sdXRpb25zLmNvbTo5MDMxIn0.KegeVAJG4HrHTYvrFllLPds0e8VxFJl-Ho4sV03G0Xcpcv0QC_4BBjy9XYjR61R4GS9i9b04d9b4EBLZJQwGGZctP0x36TSVsamSU7DshfqSXnZgeQPa7cpR2SSNwoIblViqpDpkr50nlRvXz1WGqJHRcvBzoMQ9F47uq_XOB3rGaftoIQ2GSxSTX9BZa71hcTUJqG27lI34PcmSPJc6wVnGE0cv2qbMEfK3Wm7EHCdkxpkT52ZZXk5mH8qH4hYzDWPDemCnPrwZSuOzEkpS8qIWQYm4uOs5vPPzPz_S-lr-YFkay8VsoqVmpGyNEcqwaTncOusFTHJJvypa0cfIeA&token_type=Bearer&expires_in=7199

saveIFS=$IFS
IFS='&='
parm=($LocationHdr)
IFS=$saveIFS
echo ${parm[1]}


