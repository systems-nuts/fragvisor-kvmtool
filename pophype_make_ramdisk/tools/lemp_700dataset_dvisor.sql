

================
= my
====
syntax: cannot take '`' or '''
'''''

mysql$
DROP TABLE travel_list.places;

mysql$
CREATE TABLE travel_list.places (
  id INT AUTO_INCREMENT,
  name varchar(255),
  visited varchar(255) default NULL,
  PRIMARY KEY (id)
) AUTO_INCREMENT=1;

mysql$
INSERT INTO travel_list.places (name,visited) VALUES ("Carlton","1"),("Bearberry","1"),("Utrecht","0"),("Wolkrange","1"),("Hasselt","0"),("Valéncia","1"),("Sahagún","1"),("Asansol","1"),("Woutersbrakel","1"),("Samaniego","1"),("Tebing Tinggi","1"),("Roccasicura","0"),("Nil-Saint-Vincent-Saint-Martin","1"),("East Jakarta","1"),("Morrinsville","1"),("Minna","0"),("Klabbeek","0"),("Veerle","1"),("Vaux-lez-Rosieres","1"),("Banda Aceh","1"),("Ramskapelle","0"),("Guadalupe","1"),("Trieste","1"),("Chiauci","0"),("La Hulpe","1"),("Pemberton","0"),("Aurora","1"),("Jemappes","1"),("Poggiodomo","1"),("Tielrode","0"),("Haverfordwest","1"),("Mapiripana","0"),("Baton Rouge","1"),("Gandhinagar","1"),("New Haven","0"),("Tourcoing","0"),("Grand Rapids","0"),("Ensenada","1"),("Piana degli Albanesi","0"),("Nizip","1"),("Oryol","0"),("Cerrillos","1"),("Colombo","0"),("Ipiales","0"),("Kingston","0"),("Herfelingen","0"),("Auvelais","1"),("Heusden-Zolder","0"),("Salt Spring Island","0"),("Bikaner","0"),("Fort St. John","1"),("Płock","0"),("Portland","1"),("Kamyzyak","0"),("Latera","1"),("Andernach","0"),("Bharuch","1"),("Market Drayton","1"),("El Quisco","1"),("Quinchao","0"),("Merrickville-Wolford","1"),("Mohmand Agency","0"),("Golspie","0"),("Salvador","1"),("Calder","1"),("Milwaukee","0"),("Camborne","1"),("Maizeret","1"),("Brandon","0"),("Alto Biobío","0"),("Poppel","0"),("Valparaíso de Goiás","1"),("Moliterno","1"),("Dublin","0"),("Hinckley","1"),("Verzegnis","0"),("Drancy","1"),("Cisnes","1"),("Nairn","0"),("Lamont","0"),("Fontainelque","1"),("Etawah","0"),("Laren","0"),("Leduc","1"),("Sale","0"),("Trivandrum","1"),("Aizwal","1"),("Aurora","1"),("Treglio","1"),("Chiclayo","0"),("Hualaihué","1"),("Bierges","0"),("Wimbledon","1"),("Verdun","1"),("Noduwez","1"),("Picton","0"),("Lapscheure","1"),("Stranraer","0"),("Marystown","0"),("Bridgeport","1");

<copy past 7 times>

MariaDB [(none)]> SELECT * FROM travel_list.places;
+-----+--------------------------------+---------+
| id  | name                           | visited |
+-----+--------------------------------+---------+
|   1 | Carlton                        | 1       |
|   2 | Bearberry                      | 1       |
|   3 | Utrecht                        | 0       |
|   4 | Wolkrange                      | 1       |
|   5 | Hasselt                        | 0       |
....
| 692 | Bierges                        | 0       |
| 693 | Wimbledon                      | 1       |
| 694 | Verdun                         | 1       |
| 695 | Noduwez                        | 1       |
| 696 | Picton                         | 0       |
| 697 | Lapscheure                     | 1       |
| 698 | Stranraer                      | 0       |
| 699 | Marystown                      | 0       |
| 700 | Bridgeport                     | 1       |
+-----+--------------------------------+---------+
700 rows in set (0.00 sec)



================
= correct
====

$ mysql -u travel_user -p

mysql$ SHOW DATABASES;
Output
+--------------------+
| Database           |
+--------------------+
| information_schema |
| travel_list        |
+--------------------+
2 rows in set (0.01 sec)

Next, create a "table" named places in the "travel_list" database. From the MySQL console, run the following statement:

mysql$
CREATE TABLE travel_list.places (
    id INT AUTO_INCREMENT,
    name VARCHAR(255),
    visited BOOLEAN,
    PRIMARY KEY(id)
);

mysql$
INSERT INTO travel_list.places (name, visited) 
VALUES ("Tokyo", false),
("Budapest", true),
("Nairobi", false),
("Berlin", true),
("Lisbon", true),
("Denver", false),
("Moscow", false),
("Oslo", false),
("Rio", true),
("Cincinati", false),
("Helsinki", false);

mysql$
SELECT * FROM travel_list.places;
Output
+----+-----------+---------+
| id | name      | visited |
+----+-----------+---------+
|  1 | Tokyo     |       0 |
|  2 | Budapest  |       1 |
|  3 | Nairobi   |       0 |
|  4 | Berlin    |       1 |
|  5 | Lisbon    |       1 |
|  6 | Denver    |       0 |
|  7 | Moscow    |       0 |
|  8 | Oslo      |       0 |
|  9 | Rio       |       1 |
| 10 | Cincinati |       0 |
| 11 | Helsinki  |       0 |
+----+-----------+---------+
11 rows in set (0.00 sec)

mysql$ exit



======================
= webpage generate
====

DROP TABLE places;

CREATE TABLE myTable (
  id mediumint(8) unsigned NOT NULL auto_increment,
  name varchar(255),
  visited varchar(255) default NULL,
  PRIMARY KEY (id)
) AUTO_INCREMENT=1;

INSERT INTO myTable (name,visited) VALUES ("Carlton","1"),("Bearberry","1"),("Utrecht","0"),("Wolkrange","1"),("Hasselt","0"),("Valéncia","1"),("Sahagún","1"),("Asansol","1"),("Woutersbrakel","1"),("Samaniego","1"),("Tebing Tinggi","1"),("Roccasicura","0"),("Nil-Saint-Vincent-Saint-Martin","1"),("East Jakarta","1"),("Morrinsville","1"),("Minna","0"),("Klabbeek","0"),("Veerle","1"),("Vaux-lez-Rosieres","1"),("Banda Aceh","1"),("Ramskapelle","0"),("Guadalupe","1"),("Trieste","1"),("Chiauci","0"),("La Hulpe","1"),("Pemberton","0"),("Aurora","1"),("Jemappes","1"),("Poggiodomo","1"),("Tielrode","0"),("Haverfordwest","1"),("Mapiripana","0"),("Baton Rouge","1"),("Gandhinagar","1"),("New Haven","0"),("Tourcoing","0"),("Grand Rapids","0"),("Ensenada","1"),("Piana degli Albanesi","0"),("Nizip","1"),("Oryol","0"),("Cerrillos","1"),("Colombo","0"),("Ipiales","0"),("Kingston","0"),("Herfelingen","0"),("Auvelais","1"),("Heusden-Zolder","0"),("Salt Spring Island","0"),("Bikaner","0"),("Fort St. John","1"),("Płock","0"),("Portland","1"),("Kamyzyak","0"),("Latera","1"),("Andernach","0"),("Bharuch","1"),("Market Drayton","1"),("El Quisco","1"),("Quinchao","0"),("Merrickville-Wolford","1"),("Mohmand Agency","0"),("Golspie","0"),("Salvador","1"),("Calder","1"),("Milwaukee","0"),("Camborne","1"),("Maizeret","1"),("Brandon","0"),("Alto Biobío","0"),("Poppel","0"),("Valparaíso de Goiás","1"),("Moliterno","1"),("Dublin","0"),("Hinckley","1"),("Verzegnis","0"),("Drancy","1"),("Cisnes","1"),("Nairn","0"),("Lamont","0"),("Fontainelque","1"),("Etawah","0"),("Laren","0"),("Leduc","1"),("Sale","0"),("Trivandrum","1"),("Aizwal","1"),("Aurora","1"),("Treglio","1"),("Chiclayo","0"),("Hualaihué","1"),("Bierges","0"),("Wimbledon","1"),("Verdun","1"),("Noduwez","1"),("Picton","0"),("Lapscheure","1"),("Stranraer","0"),("Marystown","0"),("Bridgeport","1");

INSERT INTO myTable (name,visited) VALUES ("TrognŽe","0"),("Chiusa/Klausen","0"),("Indore","0"),("Qualicum Beach","1"),("Pozzuolo del Friuli","0"),("Gboko","0"),("Faisalabad","0"),("Monte San Pietrangeli","0"),("Meduno","0"),("Rockford","0"),("Sneek","0"),("Waterbury","1"),("Cisterna di Latina","0"),("Huntsville","1"),("Hallaar","0"),("Feira de Santana","0"),("Zeist","0"),("Bengkulu","0"),("Catanzaro","1"),("Villa Latina","0"),("Montague","1"),("Martelange","1"),("Fort Wayne","1"),("Daejeon","0"),("Jennersdorf","1"),("Tonk","0"),("Zoetermeer","0"),("Pasuruan","0"),("Quintero","0"),("Richmond","1"),("Anghiari","0"),("Geoje","0"),("Caprauna","0"),("Gruitrode","0"),("Mollem","1"),("Brisbane","0"),("Barrhead","0"),("Pollein","1"),("Santa Rosa de Cabal","1"),("Gbongan","0"),("Bolinne","0"),("Cheltenham","1"),("Stockport","1"),("Navsari","1"),("Amaro","1"),("Waiuku","0"),("Rouvroy","0"),("Bronnitsy","0"),("Carpignano Salentino","0"),("Plast","0"),("Mexicali","1"),("Kotlas","1"),("Krems an der Donau","1"),("Sundrie","0"),("Asansol","0"),("Westmount","0"),("Delmenhorst","0"),("Aylmer","0"),("Avernas-le-Bauduin","0"),("Molina","1"),("Sincelejo","1"),("Pictou","1"),("Mansfield-et-Pontefract","1"),("Marentino","0"),("Eksaarde","1"),("Avadi","1"),("Wolfville","0"),("Nil-Saint-Vincent-Saint-Martin","0"),("East Gwillimbury","0"),("Hualañé","0"),("Anlier","0"),("Bergisch Gladbach","0"),("Orta San Giulio","1"),("Kızılcahamam","0"),("Bielefeld","1"),("Putignano","1"),("Haverhill","1"),("Ribnitz-Damgarten","1"),("Cles","0"),("Aurangabad","0"),("Paine","1"),("Wemmel","0"),("Warangal","1"),("Newquay","1"),("College","1"),("Lodelinsart","0"),("Brussel X-Luchthaven Remailing","1"),("Autre-Eglise","0"),("Joué-lès-Tours","1"),("Astrakhan","0"),("Nadrin","0"),("Saarbrücken","0"),("Ravenstein","0"),("Gambolò","0"),("Maullín","1"),("San Juan de Girón","1"),("Narbonne","1"),("Zeebrugge","0"),("Bahraich","1"),("Nieuwkapelle","0");
