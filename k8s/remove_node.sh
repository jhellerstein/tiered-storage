#!/bin/bash

if [[ -z "$1" ]] || [[ -z "$2" ]]; then
  echo "Usage ./remove_node.sh <node-type> <node-id>"
  exit 1
fi

# convert from IP to a hostname
EXTERNAL_IP=`kubectl get pods -o json | jq '.items[] | select(.status.podIP=="'$2'")' | jq '.status.hostIP' | cut -d'"' -f2`
IP=`echo $EXTERNAL_IP | sed "s|\.|-|g"`
HN=ip-$IP.ec2.internal

# retrieve by host name and get the unique id
LABEL=`kubectl get node -l kubernetes.io/hostname=$HN -o jsonpath='{.items[*].metadata.labels.role}' | cut -d- -f2`

if [ "$1" = "m" ]; then
  INST_NAME="memory-instance-$LABEL"
elif [ "$1" = "e" ]; then
  EBS_VOLS=`kubectl get pod ebs-instance-$LABEL -o jsonpath='{.spec.volumes[*].awsElasticBlockStore.volumeID}'`

  INST_NAME="ebs-instance-$LABEL"
else 
  echo "Unrecognized node type: $1."
  exit 1
fi

# delete the pod and instance groups
kubectl delete pod $INST_NAME
kops delete instancegroup $INST_NAME --yes

# if we're dropping an ebs instance, delete the volume
if [ "$1" = "e" ]; then 
  for vol in $EBS_VOLS; do
    aws ec2 delete-volume --volume-id $vol
  done
fi
