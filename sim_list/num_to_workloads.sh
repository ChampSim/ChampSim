num=0

while read line
do
    let num=num+1

    line1=`sed -n ''$num'p' 4core_number.txt | awk '{print $1}'`
    line2=`sed -n ''$num'p' 4core_number.txt | awk '{print $2}'`
    line3=`sed -n ''$num'p' 4core_number.txt | awk '{print $3}'`
    line4=`sed -n ''$num'p' 4core_number.txt | awk '{print $4}'`

    app1=`sed -n ''$line1'p' crc2_list.txt`
    app2=`sed -n ''$line2'p' crc2_list.txt`
    app3=`sed -n ''$line3'p' crc2_list.txt`
    app4=`sed -n ''$line4'p' crc2_list.txt`
    echo $app1 $app2 $app3 $app4
done < 4core_number.txt
