#!/usr/bin/python

import os
import time
import csv
import sqlite3
import pygraphviz as pgv

lqicsv = "lqi-names.csv"
lqidb = "/run/shm/lqi.db"
zlldb = "/home/pi/.local/share/dresden-elektronik/deCONZ/zll.db"

G = pgv.AGraph(strict=False, directed=True)
    
nameslist = {}

if not os.path.exists(lqicsv):
    if os.path.exists(zlldb):
        print "reading database zll.db"
        db = sqlite3.connect(zlldb)
        sql = """SELECT upper(replace(d.mac, ':', '')) as addr, n.name, n.manufacturername, n.modelid  
        FROM devices d
        LEFT JOIN nodes n
        on d.mac = substr(n.mac,0,24)
        WHERE n.name NOTNULL
        GROUP by d.mac
        UNION
        SELECT upper(replace(d.mac, ':', '')) as addr, s.name, s.manufacturername, s.modelid 
        FROM devices d
        LEFT JOIN sensors s
        on d.mac = substr(s.uniqueid,0,24) 
        WHERE d.mac NOT in (
                              SELECT substr(mac,0,24) 
                              FROM nodes
                              )
        GROUP by substr(s.uniqueid,0,24) ;
        """
        cursor = db.cursor()
        cursor.execute(sql)
        result = cursor.fetchall()
        print result
        print "writing file "+ lqicsv
        with open(lqicsv, "wt") as csvNamesWrite:
            writer = csv.writer(csvNamesWrite, delimiter='|')
            for row in result:
                writer.writerows([row])

if os.path.exists(lqicsv):
    with open(lqicsv, 'rt') as csvNamesFile:
        csvNames = csv.reader(csvNamesFile, delimiter='|')
        #headers = next(csvNames)
        for row in csvNames:
            addr = row[0]
            name = row[1]
            nameslist[addr] = name

print nameslist

if os.path.exists(lqidb):
    db = sqlite3.connect(lqidb)
    sql = """SELECT srcAddr, neighborExtAddr, neighborExtPanId, relationship, lqiLinkQuality, depth
    FROM lqi
    WHERE neighborExtAddr is NOT 'FFFFFFFFFFFFFFFF'
    ORDER by depth;
    """
    cursor = db.cursor()
    cursor.execute(sql)
    result = cursor.fetchall()
    print "writing file lqi.csv"
    with open("lqi.csv", "wt") as csvLqiWrite:
        writer = csv.writer(csvLqiWrite, delimiter='|')
        for row in result:
            writer.writerows([row])  

with open('lqi.csv', 'rt') as csvInFile:
    csvIn = csv.reader(csvInFile, delimiter='|')
    #headers = next(csvIn)
    neighbor = [row for row in csvIn]

coordinator = neighbor[0][2]
coordinater_name = "deCONZ Gateway"
nameslist[coordinator] = coordinater_name

parentlist = set()
childlist = set()

G.add_node(coordinator, label=coordinater_name, color='blue', pin='true')

# find neighbor
for x in neighbor:
    parent = x[0]
    child = x[1]
    relation = x[3]
    linkqual = x[4]
    depth = int(x[5])
    edgelabel = linkqual
    edgecolor = 'grey'
    edgestyle = 'solid'
    if relation == 'child':
        edgecolor = 'green'
    elif relation == 'previous_child':
        edgecolor = 'yellow'
    elif relation == 'parent':
        edgecolor = 'orange'
    elif relation == 'sibling':
        edgestyle = 'dashed'
        edgelabel = ''
    else:
        edgelabel = ''
    
    if coordinator == parent:
        edgecolor = 'blue'
    
    parentlist.add(parent)
    childlist.add(child)
    parentlabel = parent
    if parent in nameslist:
        parentlabel = nameslist[parent]
    childlabel = child
    if child in nameslist:
        childlabel = nameslist[child]
    
    G.add_node(parent, label=parentlabel, pin='true')
    G.add_node(child, label=childlabel, pin='true')
        
    if G.has_edge(parent, child):
        ed = G.get_edge(parent, child)
        ed.attr['dir'] = 'both'
    elif G.has_edge(child, parent):
        ed = G.get_edge(child, parent)
        ed.attr['dir'] = 'both'
    else:
        print "add relation", relation, ":", parentlabel, "-", childlabel, depth, linkqual
        G.add_edge(parent, child, color=edgecolor, style=edgestyle, label=edgelabel)

totalp = len(parentlist)
totalc = len(childlist)
print "total parent nodes", str(totalp)
print "total child nodes", str(totalc)

# add nodes from zll.db that are missing in lqi.db
nodelist = list(nameslist)
for node in nodelist:
    if node not in parentlist and node not in childlist:
        print "add missing node "+ nameslist[node], node
        G.add_node(node, label=nameslist[node], color='red', pin='true')

ts = time.localtime()
print(time.strftime("%Y-%m-%d-%H:%M:%S", ts))
pngfile = "lqi-" + time.strftime("%Y-%m-%d-%H.%M.%S", ts) + ".png"

#G.write("lqi.dot")

# http://pygraphviz.github.io/documentation/pygraphviz-1.5/reference/agraph.html
# https://graphviz.gitlab.io/_pages/doc/info/attrs.html
G.graph_attr['label'] = "deCONZ Zigbee mesh network "+ time.strftime("%d.%m.%Y %H:%M")
G.graph_attr['overlap'] = 'prism'
G.graph_attr['root'] = coordinator
G.graph_attr['splines'] = 'true'
G.layout(prog='twopi') # prog=['neato'|'dot'|'twopi'|'circo'|'fdp'|'nop'] 

G.draw(pngfile) 

os.system("ln -snf "+ pngfile +" lqi.png")
