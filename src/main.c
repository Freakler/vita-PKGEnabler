/*
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
 
 
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/display.h>
#include <psp2/apputil.h>
#include <psp2/sysmodule.h>
#include <psp2/io/fcntl.h>
#include <psp2/power.h> 
#include <psp2/vshbridge.h> 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "print/pspdebug.h"
#include "libsqlite/sqlite.h"
#include "sqlite3.h"


#define VERSION "v1.00"

#define printf psvDebugScreenPrintf 

static char* db_path = "ur0:shell/db/app.db";
char sql[256]; //SQL string



int query_get_int(char* querrystr) { //returns value, -1 on error

	int ret = -1, rc;
	sqlite3 *db        = NULL;  //Database handle
	sqlite3_stmt *stmt = NULL;	//Statement handle
	char *errmsg       = NULL;
	
	sceSysmoduleLoadModule(SCE_SYSMODULE_SQLITE);
	sqlite3_rw_init();
	
	SceUID fd = sceIoOpen(db_path, SCE_O_RDONLY, 0777);
	if (fd > 0) {
		sceIoClose(fd);
		rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
	} else 
		return -1; //error opening file
	
	
	rc = sqlite3_prepare_v2(db, querrystr, -1, &stmt, NULL);
	if (rc != SQLITE_OK)
		goto out;
	
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW)
		goto out;
	
	ret = sqlite3_column_int(stmt, 0);
	rc = sqlite3_finalize(stmt);
	
	out:
		if (rc != SQLITE_OK) 
			printf("ERROR %d: %s\n", rc, (errmsg != NULL) ? errmsg : sqlite3_errmsg(db));

		sqlite3_free(errmsg);
		if (db != NULL)
			sqlite3_close(db);
		
		sqlite3_rw_exit();
	
	return ret;
}

int query_exec(char* querrystr) { //returns 1 on success, -1 on error
	
	int ret = -1, rc;
	sqlite3 *db        = NULL;  //Database handle
	sqlite3_stmt *stmt = NULL;	//Statement handle
	char *errmsg       = NULL;
	
	sceSysmoduleLoadModule(SCE_SYSMODULE_SQLITE);
	sqlite3_rw_init();
	
	SceUID fd = sceIoOpen(db_path, SCE_O_RDONLY, 0777);
	if (fd > 0) {
		sceIoClose(fd);
		rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE, NULL);
	} else 
		return -1; //error opening file
	
	rc = sqlite3_exec(db, querrystr, NULL, NULL, &errmsg);
	if (rc == SQLITE_OK) {
		ret = 1;
	} else {
		printf("ERROR %d: %s\n", rc, (errmsg != NULL) ? errmsg : sqlite3_errmsg(db));
		ret = -1;
	}
		
	sqlite3_free(errmsg);
	if (db != NULL)
		sqlite3_close(db);
		
	sqlite3_rw_exit();
	
	return ret;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define TRANSFER_SIZE 64 * 1024 //64kb

int copyFile(char *src_path, char *dst_path) { //thx Flow VitaShell
	// The source and destination paths are identical
	if (strcmp(src_path, dst_path) == 0) return -1;

	// The destination is a subfolder of the source folder
	int len = strlen(src_path);
	if (strncmp(src_path, dst_path, len) == 0 && dst_path[len] == '/') return -1;

	SceUID fdsrc = sceIoOpen(src_path, SCE_O_RDONLY, 0);
	if (fdsrc < 0) return fdsrc;

	SceUID fddst = sceIoOpen(dst_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fddst < 0) {
		sceIoClose(fdsrc);
		return fddst;
	}

	void *buf = malloc(TRANSFER_SIZE);

	int read;
	while ((read = sceIoRead(fdsrc, buf, TRANSFER_SIZE)) > 0) {
		sceIoWrite(fddst, buf, read);
	}

	free(buf);

	sceIoClose(fddst);
	sceIoClose(fdsrc);

	return 0;
}

int doesFileExist(const char *file) {
  SceUID fd = sceIoOpen(file, SCE_O_RDONLY, 0);
  if( fd < 0 ) 
	  return -1;

  sceIoClose(fd);
  return 0;
}

int writeToConfig(char* taiconfig, char* category, char* pluginstring) { //its-a-freaggin-mess!
	
	int found = -1, skip = 0, categorywindow = -1;	
	char buffer[1024], pathbuff[1024];
	
	FILE *file = fopen(taiconfig, "r");
	FILE *temp = fopen("ur0:tai/temp.txt", "w");
	
	
	if (file == NULL){
		//printf("error opening tai config\n");
		return -1; //Error: Couldn't open for writing
		
	} else {

		while(fgets(buffer, sizeof(buffer), file) != NULL) {
			
			if( buffer[0] != '\n' )	{ //write
				
				if( buffer[0] == '*' ) { //its a category!
					
					if( strstr(buffer, category) == NULL && found != 1 && categorywindow == 1 ) { //worked all the way through correct category but not found any commented out version -> add now 
						fprintf(temp, "%s\n", pluginstring); //write plugin (after not found)
						found = 1;
					} 		
					
					if( strstr(buffer, category) != NULL ) { //found category to write plugin to!
						categorywindow = 1;
					} else 
						categorywindow = 0;
					
					fprintf(temp, "\n"); //add newline for readability between categories
				
				} else {
					if( found != 1 && categorywindow == 1 ) { //inside category to write plugin to! 
						if( strstr(buffer, pluginstring) != NULL ) { //check for commented out or exisiting
							fprintf(temp, "%s\n", pluginstring); //write plugin (comment out)
							skip = 1;
							found = 1;
						}
					} 	
				}
				
				if(skip == 0) { //another badass fix
					fprintf(temp, "%s", buffer); //write normal line
				} else skip = 0;
				
			}
				
			if( buffer[strlen(buffer)-1] != '\n' ) //whitespaces fix thingy
				fprintf(temp, "\n"); 
			
		}
		
		//category didn't exist yet -> append to the end
		if( found != 1 ) {
			if( categorywindow == 0 ) //only important when correct category is the last category (check "worked all the way.. " not executed because no category after) yes, ugly
				fprintf(temp, "\n*%s\n", category); //write category
			fprintf(temp, "%s\n", pluginstring); //write plugin
		}
			
	}
	
	fclose(file);
	fclose(temp);
	
	sceIoRemove(taiconfig);
	copyFile("ur0:tai/temp.txt", taiconfig);
	sceIoRemove("ur0:tai/temp.txt");
	
	return 0;
}

int removeFromConfig(char* taiconfig, char* category, char* pluginstring) {
	
	int categorywindow = -1;
	char buffer[1024], pathbuff[1024];
	FILE *file = fopen(taiconfig, "r");
	FILE *temp = fopen("ur0:tai/temp.txt", "w");
	
	
	if (file == NULL){
		//printf("error opening tai config\n");
		return 1; //Error: Couldn't open for writing
	} else {
		
		while( fgets(buffer, sizeof(buffer), file) != NULL ) {
			
			if( buffer[0] == '*' ) { //its a category!
				if( strstr(buffer, category) != NULL ) { //found category to write plugin to!
					categorywindow = 1;
				} else categorywindow = 0;
			}
			
			if( categorywindow ) {
				if( strstr(buffer, pluginstring) != NULL )  //found correct plugin line
					fprintf(temp, "#"); //comment out (TODO multiple #)
			}
			
			fprintf(temp, buffer); //copy
		}

	
	}
	fclose(file);
	fclose(temp);
	
	sceIoRemove(taiconfig);
	copyFile("ur0:tai/temp.txt", taiconfig);
	sceIoRemove("ur0:tai/temp.txt");
	
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int doesAppIconWithTitleIdExist(char* id) { 
	snprintf(sql, sizeof(sql), "SELECT EXISTS(SELECT 1 FROM tbl_appinfo_icon WHERE titleId='%s')", id);
	return query_get_int(sql);
}

int removeAppIconWithTitleId(char* id) { 	
	snprintf(sql, sizeof(sql), "DELETE FROM tbl_appinfo_icon WHERE titleId='%s'", id);
	return query_exec(sql);
}

int createPackageInstallerBubbleAt(int page, int pos) {
	snprintf(sql, sizeof(sql), "INSERT INTO tbl_appinfo_icon VALUES(%d,%d,'vs0:app/NPXS10031/sce_sys/icon0.png','★Package Installer',0,NULL,'NPXS10031',5,0,0,NULL,NULL,NULL,NULL,NULL)", page, pos);
	return query_exec(sql);
}


int createNewLiveAreaPage() {
	
	int ret = -1, pageId = -1, pageNo = -1;
		
	snprintf(sql, sizeof(sql), "SELECT MAX(pageId) FROM tbl_appinfo_page");
	pageId = query_get_int(sql) + 1;
	snprintf(sql, sizeof(sql), "SELECT MAX(pageNo) FROM tbl_appinfo_page");
	pageNo = query_get_int(sql) + 1;
	
	//printf("pageId -> %i\n", pageId);
	//printf("pageNo -> %i\n", pageNo);
	
	if( pageId > 0 && pageNo > 0 ){
		snprintf(sql, sizeof(sql), "INSERT INTO tbl_appinfo_page VALUES(%d,%d,NULL,0,0,0,0,0,33554431,NULL,NULL,NULL,NULL)", pageId, pageNo);
		ret = query_exec(sql);
		if( ret > 0) //page created successfuly
			return pageId;
	} 

	return -1; //error
}

int getFreePage() {
	
	int page = -1;
	
	//snprintf(sql, sizeof(sql), "SELECT MAX(pageId) FROM tbl_appinfo_icon"); //this one includes folders
	snprintf(sql, sizeof(sql), "SELECT MAX(pageId) FROM tbl_appinfo_page WHERE pageNo > 0"); //not 100% save either
	page = query_get_int(sql);
	if( page < 0 ) //error
		return -1;	
		
	return page;
}

int getFreePosForPage(int page) {
	snprintf(sql, sizeof(sql), "SELECT MAX(pos) FROM tbl_appinfo_icon WHERE pageId=%d", page);
	return query_get_int(sql);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) {
	
	int ret = -1;
	SceCtrlData pad, oldpad;
	oldpad.buttons = 0;
	psvDebugScreenInit();
	
	
	psvDebugScreenSetXY(0, 1);
	psvDebugScreenSetTextColor(0xFF00FFFF); //YELLOW
	psvDebugScreenPrintf("PKGEnabler %s\n", VERSION);	
	
	psvDebugScreenSetXY(0, 3);
	psvDebugScreenSetTextColor(0xFFFFFFFF); //WHITE
	psvDebugScreenPrintf("This program will enable the normally hidden PackageInstaller bubbleon the LiveArea and restore basic functionality via plugin installedto the tai config!\n\n\n");	
	
	
	if( vshSblAimgrIsCEX() ) { //this is only for retail devices
		ret = doesAppIconWithTitleIdExist("NPXS10031");	//check for previous installation
		if( ret == 0 ) {
			psvDebugScreenPrintf("Press SQUARE to install..\n\n");	
		} else if( ret == 1 ) {
			psvDebugScreenPrintf("Press SQUARE to uninstall and restore defaults..\n\n");	
		} else {
			psvDebugScreenPrintf("Something is wrong.. aborting..\n\n");	
			goto exit;
		}
	} else 
		goto exit;
	
	
	psvDebugScreenSetXY(0, 13);
	psvDebugScreenSetTextColor(0xFF00FF00); //GREEN
	
	
	while(1) {
		memset(&pad, 0, sizeof(pad));
		sceCtrlPeekBufferPositive(0, &pad, 1);
		
		 if( pad.buttons != oldpad.buttons ) {
		 
			if( pad.buttons & SCE_CTRL_SQUARE ) {			
				
				//printf("Checking for existing PKG Installer Bubble.. ");
				ret = doesAppIconWithTitleIdExist("NPXS10031");	
				if( ret == 0 ) { //INSTALL
					
					/// find free space for bubble to be created
					int page = getFreePage(); //printf("Found page %d \n", page);
					if( page < 0 ) goto exit;
					int pos = getFreePosForPage(page);
					if( pos < 0 ) goto exit;
					
					if(pos == 9) { //full page detected -> create bubble on new page
						pos = 0;
						printf("> Creating new LiveArea page.. ");
						page = createNewLiveAreaPage();
						if( page < 0 ) {
							printf("error!\n\n");
							goto exit;
						} else printf("%d\n\n", page);
					
					} else pos++;
					printf("> Found free space on page %d at position %d\n\n", page, pos);
					
					/// create the actual bubble
					printf("> Creating PKG Installer Bubble.. ");	
					ret = createPackageInstallerBubbleAt(page, pos);
					if( ret == -1 ) {
						printf("error!\n\n");
						goto exit;
					} else printf("success!\n\n");
					
					
					///copy plugin
					printf("> Installing plugin.. ");
					ret = copyFile("app0:plugin/pkgenabler.suprx", "ur0:tai/pkgenabler.suprx");
					if( ret < 0 ) {
						printf("error!\n\n");
						goto exit;
					} else printf("success!\n\n");
					
					///write to tai config
					printf("> Writing tai config.. ");
					ret = writeToConfig("ur0:tai/config.txt", "NPXS10031", "ur0:tai/pkgenabler.suprx"); //TODO ux0 ur0
					if( ret == -1 ) {
						printf("error!\n\n");
						goto exit;
					} else printf("success!\n\n");			
					
					goto exit_confirm;
					
				
				} else if( ret == 1 ) { //UNINSTALL
					
					printf("> Removing PKG Installer Bubble.. ");	
					ret = removeAppIconWithTitleId("NPXS10031");	
					if( ret == 1 ) {
						printf("success!\n\n");
						
						///delete suprx
						printf("> Removing plugin.. ");
						if( doesFileExist("ur0:tai/pkgenabler.suprx") < 0 ) {
							printf("not found!\n\n");
						} else {
							sceIoRemove("ur0:tai/pkgenabler.suprx"); //todo error handling
							printf("success!\n\n");
						}
						
						///remove plugin from tai config
						printf("> Writing tai config.. ");
						ret = removeFromConfig("ur0:tai/config.txt", "NPXS10031", "ur0:tai/pkgenabler.suprx"); //TODO ux0 ur0
						if( ret == -1 ) {
							printf("error!\n\n");
							goto exit;
						} else printf("success!\n\n");	
						
						goto exit_confirm;
						
					} else printf("error!\n\n");
				
				} else { //ERROR
					printf("error!\n\n");
					goto exit;
				}
			}

			
			if( pad.buttons & SCE_CTRL_CIRCLE ) {		
				
			}
			
			oldpad = pad;
		 }

		
		/// exit combo
		if( (pad.buttons & SCE_CTRL_START) && (pad.buttons & SCE_CTRL_SELECT) )
			break;
		
		if( pad.buttons & SCE_CTRL_TRIANGLE )
			break;
		
	}
	
	exit:
		sceKernelDelayThread(1 * 1000 * 1000); //wait 1 second
		printf("\n\nPress X to exit..");
		do {
			sceCtrlPeekBufferPositive(0, &pad, 1);
		} while( !(pad.buttons & SCE_CTRL_CROSS) );
		sceKernelExitProcess(0);
		return 0;
	
	exit_confirm:
		sceKernelDelayThread(1 * 1000 * 1000); //wait 1 second
		printf("\n\nPress X to reboot..");
		do {
			sceCtrlPeekBufferPositive(0, &pad, 1);
		} while( !(pad.buttons & SCE_CTRL_CROSS) );
		scePowerRequestColdReset();
		sceKernelExitProcess(0);
		return 0;
}
