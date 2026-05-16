
template <typename T>class ObjectList {
  private: 
    unsigned int max;
    T *objects; 
    unsigned int pos=0; //hinter letztem Eintrag
    char * fileName = NULL; //wird der char * als Literal übergeben, ist er im Flash und im RAM - wie lange ist er da?
                           //ah ja, string literale sind automatisch statisch - daher vermutlich auch das speichern im Ram und im Flash
                           //dennoch zur sicherheit kopie
  public:
    ObjectList(unsigned int max, const char * fileName = "")
    {
      this->max = max;
      unsigned len = strlen(fileName);
      if (len)
      {
        this->fileName = new char[len+1];
        strcpy(this->fileName,fileName);
      }    
      objects = new T[max];
    }
    ~ObjectList()
    {
      delete[] objects;
      delete [] fileName; //delete on Null should be safe
    }

    unsigned int getDelimiterPos() //eins hinter dem letzten Eintrags
    {
      return pos;
    }
    void clear()
    {
      /*
      for (int i = 0 ; i < pos ; ++i)
      {
        objects[i] = Null;
      }*/
        
      pos = 0;
      return; 
    }
    void add(const T &o) //am ende einfuegen, ggf. platz frei machen 
    {
      if (pos == max) //voll
      {
        for (unsigned int i = 0; i < max-1; ++i)
          objects[i] = objects[i+1];
        pos--;  
      }
      objects[pos++] = o;
    }
    //nur hinzufuegen, wenn die id noch nicht da und liste nicht voll
    bool addNew(const T & o)
    {
      bool retVal = false;
      if (indexOfOnlyId(o) == -1  && pos < max)
      {
        add(o);
        retVal = true;    
      }
      return retVal;
    }
    //hmm, was ist mit index out of bounds ? Exception - habe zu lang kein c++ mehr programmiert ...
    T & getAt(int index)
    {
      return objects[index]; 
    }

    T * getList(unsigned int & anzahl)
    {
      anzahl = pos;
      return objects;  
    }
    void serialPrint()
    {
      Serial.println("--------------");
      for (unsigned int i = 0; i < pos ; ++i)
        Serial.println(String(objects[i]));
      Serial.println("--------------");
    }
    String htmlLines()
    {
      String result = "";
      for (unsigned int i = 0 ; i < pos ; ++i)
        result += objects[i] + "<br>";  //+ ueberladen
      return result;
    }
    
    int indexOf(const T & o)
    {
      int index = -1;
      for (int i = 0 ; i < pos ; ++i)
      {
        if (objects[i] == o)
        {
            index = i;
            break;
        }
      }
      return index;
    }
    //damit ist die Funktion nicht mehr generisch sondern sehr speziell für das rfid-Objekt 
    int indexOfOnlyId(const T & o)
    {
      int index = -1;
      for (int i = 0 ; i < pos ; ++i)
      {
        if (objects[i].id == o.id)
        {
            index = i;
            break;
        }
      }
      return index;
     
    }
    void deleteAt(int index)
    {
      if (index > -1 && index < max)
      {
        --pos;
        for (int i = index; i < pos;  ++i)
          objects[i] = objects[i+1];
      }
    }
    //gibt den Index des geloeschten zurück
    int deleteEntry(const T &o)
    {
      int index = indexOf(o);
      //Serial.print("Found to delete at ");
      //Serial.println( index);
      deleteAt(index);
      return index;
    }

    //toCheck: muss erst gemounted werden? - denke nicht
    /*
    bool loadFromFile()
    {
      Serial.println("Load from filesystem");
      bool ret = true;
      if (fileName !="")
      {
         File dataFile = LittleFS.open(fileName, "r");
         int i=0; 
         if (dataFile)
         {
            while (dataFile.available() && i < max)
            {
               objects[i++] = T(dataFile.readStringUntil('\n'),'|');
               //Serial.println("Add object"); 
            }
            pos = i;
            dataFile.close();
         }
         else
          ret = false;
      }
      return ret;
    }
    */
    //Die Strings einfach der Reihe nach heraus 
    /*
    bool saveToFile()
    {
      bool ret = true;
      //Serial.println(String("schreibe in ") +fileName);
      if (fileName !="")
      {
         File dataFile = LittleFS.open(fileName, "w");
         if (dataFile)
         {
          for (int i = 0; i < pos ; ++i)
            dataFile.println(objects[i].getAsString('|')); //scheint er so zu nehmen
          dataFile.close();
         }
         else 
          ret = false;
      }
      return ret;      
    }
    */
};
