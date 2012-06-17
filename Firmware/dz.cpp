/* Speedoino - This file is part of the firmware.
 * Copyright (C) 2011 Kolja Windeler
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"

speedo_dz::speedo_dz(void){
}

void speedo_dz::counter(){
	if(DZ_DEBUG){
		Serial.print("DZ peak@");
		Serial.print(millis());
		Serial.print(" ");
		Serial.println(peak_count);
	};
	peak_count++; // läuft bis 65.536 dann auf 0
};



void speedo_dz::calc() {
	///// DZ BERECHNUNG ////////
	unsigned long now=millis(); 		// aktuelle zeit
	unsigned long differ=now-previous_time; // zeit seit dem letzte mal abholen der daten
	unsigned int  now_peaks=peak_count; // aktueller dz zähler stand, separate var damit der peakcount weiter verndert werden koennte
	if((now<pAktors->m_stepper->time_go_full+800)){
		if(pAktors->m_stepper->init_done){
			pAktors->m_stepper->go_to(0,0);
		}
	} else if(now>pAktors->m_stepper->time_go_full+800 && !pAktors->m_stepper->init_done){ // zwischen 1 und 2 sec
		pAktors->m_stepper->go_to(0,0);
		pAktors->m_stepper->init_done=true;
		pAktors->m_stepper->time_go_full=millis()-1000;
	} else 	if(now_peaks>4 && differ>100){ // max mit 10Hz, bei niedriger drehzahl noch seltener, 1400 rpm => 680 ms
		//now_peaks=now_peaks>>anzahl_shift;			// könnte ja sein das man weniger umdrehungen als funken hat, hornet hat 2 Funken je Umdrehun
		/* bei 15krpm = 25 peaks
		 * differ => 100 --> 60000/100=600
		 * 600 * 25 => 15.000
		 *
		 * bei 1400rpm => 685 ms f
		 * r 16 Peaks
		 * 60000/685 = 88
		 * 88 * 16 = 1408
		 */
		unsigned int dz=60000/differ*now_peaks/2; // Drehzahl berechnet (60.000 weil ms => min)
		if(dz>15000){ // wenn man über 15000 U/min => Abstand von 2 Zündungen = 60000[ms/min]/15000[U/min] = 4 [ms/U]
			dz=previous_dz; // alten Wert halten, kann nicht sein
		};

		/* Timing */
		peak_count=0;
		previous_time=now; // speichere wann ich zuletzt nachgesehen habe
		previous_dz=dz; // speichere die aktuelle dz
		/* Timing */

		/* values */
		//exact=dz;
		exact=pSensors->flatIt(dz,&dz_faktor_counter,4,exact);						// IIR mit Rückführungsfaktor 3 für DZmotor
		exact_disp=pSensors->flatIt(dz,&dz_disp_faktor_counter,20,exact_disp);		// IIR mit Rückführungsfaktor 19 für Anzeige, 20*4 Pulse, 1400U/min = 2,5 sec | 14000U/min = 0,25 sec
		rounded=250*round(exact_disp/250); 											// auf 250er Runden
		/* values */

		/* gear */
		pSensors->m_gear->calc();// alle 250 ms, also mit 4Hz den Gang berechnen
		/* gear */

		/*stepper*/
		pAktors->m_stepper->go_to(exact/11.73,0); // einfach mal probieren, sonst flatit
		/*stepper*/

	} else if(now-previous_time>1000){
		rounded=0;
		exact=0;
		// zeiger
		pAktors->m_stepper->go_to(0,0);
		previous_time=now;
	};

	if(DEMO_MODE){
		if(differ>250){
			previous_time=now;
			int temp=analogRead(OIL_TEMP_PIN)-180;
			if(temp<0) temp=0;
			if (temp>600) temp=600;
			rounded=15000-temp*25;

//			int speed_me_up=50; // gut mit 50
//			if(((millis()/speed_me_up)%210)<50){
//				rounded=0;
//			} else {
//				rounded=((millis()/speed_me_up)%210)*70;
//			};
			exact=rounded;
			pSensors->m_gear->calc();
			// zeiger
			// 2 => 2*880=> 2k stepper
			pAktors->m_stepper->go_to(round(exact/11.73),0);




		}
	};
};

void helper(){
	pSensors->m_dz->counter();
}


void speedo_dz::init() {
	attachInterrupt(0, helper, RISING ); // interrupt handler für signalwechsel 0=DigiPin 2
	Serial.println("DZ init done");
	blitz_en=false;
	Serial3.flush();
};

void speedo_dz::clear_vars(){
	previous_time=millis();
	rounded=0;                 // to show on display, rounded by 50
	exact=0;                 // real rotation speed
	peak_count=0;
	blitz_dz=0;
	blitz_en=false;
	dz_faktor_counter=0;
	dz_disp_faktor_counter=0;
}

bool speedo_dz::check_vars(){
	if(blitz_dz==0){
		pDebug->sprintp(PSTR("DZ failed"));
		blitz_dz=12500; // hornet maessig
		blitz_en=true; // gehen wir mal von "an" aus
		return true;
	}
	return false;
};
