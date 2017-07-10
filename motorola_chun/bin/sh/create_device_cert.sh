#!/bin/bash
# title          : A Script to generate:
# description    : This script will do the following:
#                  1. Get the Config Params from the RA ( Registration Agent)
#                  2. Generate Key Pairs(public-private) for the tls and signing End Entities 
#                  3. Generate CSR ( Certificate Signing Request ) for the tls and signing End Entities 
#                  4. POST the CSR to the RA
#                  5. Write the Certificates for the tls and signing End Entities to files SN.tls.pem and SN.sign.pem
#author          : Krishna Ramachandra
#date            : May 18th 2016
#usage           : create_device_certs -sn <1234> -qrcodeurl <https://devregmaster.imw.motorolasolutions.com/1234
#====================================================================================================================


#### Constants

TLSEE="\"tls\""
SIGNEE="\"signing\""
CSRREQBEGIN='{"csrreq": ['
CSRBEGIN='{"csr": "'
CSREND='{"csrreq": [{"csr": "'
USAGEBEGIN='\n", "usage": '
USAGEEND='}'
CSRREQEND=']}'
TYPE="Content-Type:application/json"


#### Functions

function usage()
{
   echo "usage: create_certs [[[-sn <serial Number>] [-qrcodeurl <url>]] | [-h]]"
   exit 1
}

function validate_input()
{
   key=$1
   value=$2
   if [ -z "$value" ]; then
      echo "Missing argument for the '$key' param"
      usage
   fi
}

function get_config_params()
{
   config_params=$(curl -s -f $1)
   status=$?
   
   if [ $status != "0" ]; then
      echo "Get Config Params Failed, Invalid Qr Code URl: $1"
      exit 1
   fi
#   echo $config_params

   csr_req_url=`echo $config_params | jq -j '.configparams[0].csrparams.csrrequrl'`
### Do Some JSON Operation to Obtain the data for CSR ###
### This is for The Tls CSR ###

   tls_cert_st=`echo $config_params | jq -j '.configparams[0].csrparams.certs[0].nameattrs.st'`
   tls_cert_co=`echo $config_params | jq -j '.configparams[0].csrparams.certs[0].nameattrs.country'`
   echo $tls_cert_co > tmp_country.txt
   tls_cert_l=`echo $config_params | jq -j '.configparams[0].csrparams.certs[0].nameattrs.l'`
   tls_cert_ou=`echo $config_params | jq -j '.configparams[0].csrparams.certs[0].nameattrs.ou'`
   tls_cert_o=`echo $config_params | jq -j '.configparams[0].csrparams.certs[0].nameattrs.o'`


### This is for The Signing CSR ###

   sign_cert_st=`echo $config_params | jq -j '.configparams[0].csrparams.certs[1].nameattrs.st'`
   sign_cert_co=`echo $config_params | jq -j '.configparams[0].csrparams.certs[1].nameattrs.country'`
   sign_cert_l=`echo $config_params | jq -j '.configparams[0].csrparams.certs[1].nameattrs.l'`
   sign_cert_ou=`echo $config_params | jq -j '.configparams[0].csrparams.certs[1].nameattrs.ou'`
   sign_cert_o=`echo $config_params | jq -j '.configparams[0].csrparams.certs[1].nameattrs.o'`

### Get the Idm and vault Url ###

   idm_url=`echo $config_params | jq -j '.configparams[1].idmurl.baseurl'`
   upload_url=`echo $config_params | jq -j '.configparams[2].vaulturl.baseurl'`

}

function generate_key_pair_csr()
{
### Generate the KeyPair and CSR ###
   echo "******     GENERATING Key Pairs and CSR - ST........ ******"
   tls_key_file=$sn"_tls_key.key"
   sign_key_file=$sn"_sign_key.key"
   
   echo "openssl req -nodes -newkey rsa:2048 -keyout $tls_key_file -out $tls_csr_file -outform PEM -subj \"/C=$tls_cert_co/ST=$tls_cert_st/L=$tls_cert_l/O=$tls_cert_o/OU=$tls_cert_ou/CN=$sn\" "
   echo "openssl req -nodes -newkey rsa:2048 -keyout $sign_key_file -out $sign_csr_file -outform PEM -subj \"/C=$sign_cert_co/ST=$sign_cert_st/L=$sign_cert_l/O=$sign_cert_o/OU=$sign_cert_ou/CN=$sn\" "

   `openssl req -nodes -newkey rsa:2048 -keyout $tls_key_file -out $tls_csr_file -outform PEM -subj "/C=$tls_cert_co/ST=$tls_cert_st/L=$tls_cert_l/O=$tls_cert_o/OU=$tls_cert_ou/CN=$sn" 2>/dev/null`
   `openssl req -nodes -newkey rsa:2048 -keyout $sign_key_file -out $sign_csr_file -outform PEM -subj "/C=$sign_cert_co/ST=$sign_cert_st/L=$sign_cert_l/O=$sign_cert_o/OU=$sign_cert_ou/CN=$sn" 2>/dev/null`

}

function create_csr_req()
{
   echo "******     CREATING CSR MESSAGE........ ******"
### Create the JSON Object to Perform a POST to Get the Certificates ###
###

   while read -r line || [[ -n "$line" ]]; do
       newdata="$line\n"
       tls="$tls$newdata"
   done < $tls_csr_file

   while read -r line || [[ -n "$line" ]]; do
       newdata="$line\n"
       sign="$sign$newdata"
   done < $sign_csr_file

   json_data="$CSRREQBEGIN$CSRBEGIN"
   json_data="$json_data$tls"
   json_data="$json_data$USAGEBEGIN"
   json_data="$json_data$TLSEE$USAGEEND, "

   json_data="$json_data$CSRBEGIN"
   json_data="$json_data$sign"
   json_data="$json_data$USAGEBEGIN"
   json_data="$json_data$SIGNEE$USAGEEND"

   json_data="$json_data$CSRREQEND"

   echo $json_data > csr_req.json
}


function send_csr_req()
{ 
   echo "******     POSTING CSR........ ******"
   cert_data=$(curl -X POST -s -f -d @csr_req.json $csr_req_url --header $TYPE)
#   curl -X POST -v -d @csr_req.json $csr_req_url --header $TYPE
   status=$?

   echo "cert_data [$csr_req_url $TYPE]  status=$status"

   if [ $status != "0" ]; then
      echo "CSR Failed, Message: $cert_data"
      exit 1
   fi

   echo $cert_data > allcerts.crt

   echo "******     GENERATING THE CERTIFICATES........ ******"

   tls_cert=`echo $cert_data | jq -j '.certs[0].cert'`
   sign_cert=`echo $cert_data | jq -j '.certs[1].cert'`
   tls_cert_file=$sn"_tls.crt"
   sign_cert_file=$sn"_sign.crt"

   echo "$tls_cert" > $tls_cert_file
   echo "$sign_cert" > $sign_cert_file

   echo "******     ALL CERTIFICATES GENERATED SUCCESSFULY........ ******"
}

#### Main
sn=
qrcodeurl=
config_params=

tls_cert_co=
tls_cert_st=
tls_cert_ou=
tls_cert_o=
tls_cert_l=
tls_key_file=
tls_csr_file="tls.csr"
tls_cert_file=

sign_cert_co=
sign_cert_st=
sign_cert_ou=
sign_cert_o=
sign_cert_l=
sign_key_file=
sign_csr_file="sign.csr"
sign_cert_file=

idm_url=
upload_url=

while [[ "$1" != "" ]]; do 
   case $1 in
     -sn | --serialnumber ) validate_input $1 $2
                             shift
                             sn=$1
                             ;;
     -qrcodeurl)             validate_input $1 $2
                             shift
                             qrcodeurl=$1
                             ;;
     -h | --help )           usage
                             exit
                             ;;
     * )                     usage
                             exit 1
   esac
   shift
done


if [ -z "$sn" ]; then
  echo "Missing argument for the Serial Number parameter"
  usage
fi

if [ -z "$qrcodeurl" ]; then
  echo "Missing argument for the qrcodeurl parameter"
  usage
fi

get_config_params $qrcodeurl
generate_key_pair_csr
create_csr_req
send_csr_req
