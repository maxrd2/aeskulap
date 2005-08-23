/*
    Aeskulap ImagePool - DICOM abstraction library
    Copyright (C) 2005  Alexander Pipelka

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    Alexander Pipelka
    pipelka@teleweb.at

    Last Update:      $Author: braindead $
    Update Date:      $Date: 2005/08/23 19:31:54 $
    Source File:      $Source: /cvsroot/aeskulap/aeskulap/imagepool/Attic/DicomMoveAssociation.cpp,v $
    CVS/RCS Revision: $Revision: 1.1 $
    Status:           $State: Exp $
*/

#include "DicomNetwork.h"
#include "DicomMoveAssociation.h"

#include <diutil.h>
#include "djencode.h"
#include "djrplol.h"

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

DicomMoveAssociation::DicomMoveAssociation() {
	m_abstractSyntax = UID_MOVEPatientRootQueryRetrieveInformationModel;
	//m_abstractSyntax = UID_FINDStudyRootQueryRetrieveInformationModel;
	//m_abstractSyntaxMove = UID_MOVEStudyRootQueryRetrieveInformationModel;
	//m_ourPort = 104;		// default port
	m_maxReceivePDULength = ASC_DEFAULTMAXPDU;
}

DicomMoveAssociation::~DicomMoveAssociation() {
}

void DicomMoveAssociation::Create(const char *title, const char *peer, int port, const char *ouraet, /*int ourPort,*/ const char *abstractSyntax/*, const char *abstractSyntaxMove*/) {
	DicomAssociation::Create(title, peer, port, ouraet, abstractSyntax);
	//m_abstractSyntaxMove = abstractSyntaxMove;
	//m_ourPort = ourPort;
}

CONDITION DicomMoveAssociation::SendObject(DcmDataset *dataset) {
	return moveSCU(dataset);	
}

void DicomMoveAssociation::OnAddPresentationContext(T_ASC_Parameters *params, const char* transferSyntaxList[], int transferSyntaxListCount) {
	ASC_addPresentationContext(params, 3, m_abstractSyntax, transferSyntaxList, transferSyntaxListCount);
}

CONDITION DicomMoveAssociation::moveSCU(DcmDataset *pdset) {
	CONDITION cond;
	T_ASC_PresentationContextID presId;
	T_DIMSE_C_MoveRQ req;
	T_DIMSE_C_MoveRSP rsp;
	DIC_US msgId = assoc->nextMsgID++;
	DcmDataset* rspIds = NULL;
	const char* sopClass;
	DcmDataset* statusDetail = NULL;
	MoveCallbackInfo callbackData;

	if(pdset == NULL) {
		return DIMSE_NULLKEY;
	}

	//sopClass = m_abstractSyntaxMove;
	sopClass = m_abstractSyntax;

	// which presentation context should be used
	presId = ASC_findAcceptedPresentationContextID(assoc, sopClass);

	if (presId == 0) {
		return DIMSE_NOVALIDPRESENTATIONCONTEXTID;
	}

	callbackData.assoc = assoc;
	callbackData.presId = presId;
	callbackData.pCaller = this;

	req.MessageID = msgId;
	strcpy(req.AffectedSOPClassUID, sopClass);
	req.Priority = DIMSE_PRIORITY_HIGH;
	req.DataSetType = DIMSE_DATASET_PRESENT;
	strcpy(req.MoveDestination, m_ourAET);

	cond = DIMSE_moveUser(
						assoc,
						presId,
						&req,
						pdset,
						moveCallback,
						&callbackData,
						DIMSE_BLOCKING,
						0, 
						GetNetwork()->GetDcmtkNet(),
						subOpCallback,
						this,
						&rsp, &statusDetail, &rspIds);

	if (statusDetail != NULL) {
		printf("  Status Detail:\n");
		statusDetail->print(COUT);
		delete statusDetail;
	}

	if (rspIds != NULL) {
		delete rspIds;
	}

	return cond;
}

void DicomMoveAssociation::moveCallback(void *callbackData, T_DIMSE_C_MoveRQ *request, int responseCount, T_DIMSE_C_MoveRSP *response) {
/*	MoveCallbackInfo* myCallbackData;

	myCallbackData = (MoveCallbackInfo*)callbackData;
	DicomMoveAssociation* caller = myCallbackData->pCaller;*/
}

void DicomMoveAssociation::subOpCallback(void *pCaller, T_ASC_Network *aNet, T_ASC_Association **subAssoc) {
	DicomMoveAssociation* caller = (DicomMoveAssociation*)pCaller;

	if (caller->GetNetwork() == NULL) {
		return;
	}

	if (*subAssoc == NULL) {
		// negotiate association
		caller->acceptSubAssoc(aNet, subAssoc);
	} 
	else{
		// be a service class provider
		caller->subOpSCP(subAssoc);
	}
}

CONDITION DicomMoveAssociation::acceptSubAssoc(T_ASC_Network *aNet, T_ASC_Association **assoc) {
	CONDITION cond = ASC_NORMAL;
	const char* knownAbstractSyntaxes[] = { UID_VerificationSOPClass };
	const char* transferSyntaxes[] = { UID_JPEGProcess14SV1TransferSyntax, NULL, NULL, UID_LittleEndianImplicitTransferSyntax };

	cond = ASC_receiveAssociation(aNet, assoc, m_maxReceivePDULength);
	if (cond.bad()) {
		printf("Unable to receive association!\n");
		DimseCondition::dump(cond);
	}
	else {

		/* 
		** We prefer to accept Explicitly encoded transfer syntaxes.
		** If we are running on a Little Endian machine we prefer 
		** LittleEndianExplicitTransferSyntax to BigEndianTransferSyntax.
		*/

		/* gLocalByteOrder is defined in dcxfer.h */
		if (gLocalByteOrder == EBO_LittleEndian) {
			/* we are on a little endian machine */
			transferSyntaxes[1] = UID_LittleEndianExplicitTransferSyntax;
			transferSyntaxes[2] = UID_BigEndianExplicitTransferSyntax;
		} else {
			/* we are on a big endian machine */
			transferSyntaxes[1] = UID_BigEndianExplicitTransferSyntax;
			transferSyntaxes[2] = UID_LittleEndianExplicitTransferSyntax;
		}

		// accept the Verification SOP Class if presented */
		cond = ASC_acceptContextsWithPreferredTransferSyntaxes(
					(*assoc)->params,
					knownAbstractSyntaxes, DIM_OF(knownAbstractSyntaxes),
					transferSyntaxes, DIM_OF(transferSyntaxes));

		if (cond.good()) {
			// the array of Storage SOP Class UIDs comes from dcuid.h
			cond = ASC_acceptContextsWithPreferredTransferSyntaxes(
					(*assoc)->params,
					dcmStorageSOPClassUIDs, numberOfDcmStorageSOPClassUIDs,
					/*GetDcmStorageSOPClassUIDs(), GetNumberOfDcmStorageSOPClassUIDs(),*/
					transferSyntaxes, DIM_OF(transferSyntaxes));
		}
	}

	if (cond.good()) {
		cond = ASC_acknowledgeAssociation(*assoc);
	}

	if (cond.bad()) {
		ASC_dropAssociation(*assoc);
		ASC_destroyAssociation(assoc);
	}

	return cond;
	
}

CONDITION DicomMoveAssociation::subOpSCP(T_ASC_Association **subAssoc) {
	T_DIMSE_Message msg;
	T_ASC_PresentationContextID presID;

	/* just in case */
	if (!ASC_dataWaiting(*subAssoc, 0)) {
		return DIMSE_NODATAAVAILABLE;
	}

	OFCondition cond = DIMSE_receiveCommand(*subAssoc, DIMSE_BLOCKING, 0, &presID, &msg, NULL);

	if (cond == EC_Normal) {
		switch (msg.CommandField) {
			case DIMSE_C_STORE_RQ:
				cond = storeSCP(*subAssoc, &msg, presID);
				break;
			case DIMSE_C_ECHO_RQ:
				cond = echoSCP(*subAssoc, &msg, presID);
				break;
			default:
				cond = DIMSE_BADCOMMANDTYPE;
				break;
		}
	}

	// clean up on association termination
	if (cond == DUL_PEERREQUESTEDRELEASE) {
		cond = ASC_acknowledgeRelease(*subAssoc);
		ASC_dropSCPAssociation(*subAssoc);
		ASC_destroyAssociation(subAssoc);
		return cond;
	}
	else if (cond == DUL_PEERABORTEDASSOCIATION) {
	}
	else if (cond != EC_Normal) {
		DimseCondition::dump(cond);
		// some kind of error so abort the association
		cond = ASC_abortAssociation(*subAssoc);
	}

	if (cond != EC_Normal) {
		ASC_dropAssociation(*subAssoc);
		ASC_destroyAssociation(subAssoc);
	}
	return cond;
}

CONDITION DicomMoveAssociation::storeSCP(T_ASC_Association *assoc, T_DIMSE_Message *msg, T_ASC_PresentationContextID presID) {
	CONDITION cond;
	T_DIMSE_C_StoreRQ* req;
	DcmDataset *dset = new DcmDataset;

	req = &msg->msg.CStoreRQ;

	StoreCallbackInfo callbackData;
	callbackData.dataset = dset;
	callbackData.pCaller = this;

	cond = DIMSE_storeProvider(assoc, presID, req, (char *)NULL, 1, 
				&dset, storeSCPCallback, (void*)&callbackData,
				DIMSE_BLOCKING, 0);

	delete dset;
	return cond;
}

void DicomMoveAssociation::storeSCPCallback(void *callbackData, T_DIMSE_StoreProgress *progress, T_DIMSE_C_StoreRQ *req, char *imageFileName, DcmDataset **imageDataSet, T_DIMSE_C_StoreRSP *rsp, DcmDataset **statusDetail) {
	DIC_UI sopClass;
	DIC_UI sopInstance;
	DcmDataset* d = NULL;

	StoreCallbackInfo *cbdata = (StoreCallbackInfo*) callbackData;
	DicomMoveAssociation* caller = cbdata->pCaller;

	if (progress->state == DIMSE_StoreEnd) {
		*statusDetail = NULL;	/* no status detail */

		/* could save the image somewhere else, put it in database, etc */
		rsp->DimseStatus = STATUS_Success;

		if ((imageDataSet)&&(*imageDataSet)) {
			// do not duplicate the dataset, let the user do this
			// if he wants to

			d = cbdata->dataset;

			/*// try to compress the dataset
			//if(d->getOriginalXfer() != EXS_JPEGProcess14SV1TransferSyntax) {
				DJEncoderRegistration::registerCodecs();
				DcmXfer opt_oxferSyn(EXS_JPEGProcess14SV1TransferSyntax);

				// create RepresentationParameter
				DJ_RPLossless rp(6, 0);
				d->chooseRepresentation(EXS_JPEGProcess14SV1TransferSyntax, &rp);
				if (d->canWriteXfer(EXS_JPEGProcess14SV1TransferSyntax)) {
					//d->print(COUT, DCMTypes::PF_shortenLongTagValues);
					COUT << "DicomMoveAssociation: Output transfer syntax " << opt_oxferSyn.getXferName() << " can be written\n";
					}
				else {
					CERR << "DicomMoveAssociation: No conversion to transfer syntax " << opt_oxferSyn.getXferName() << " possible!\n";
				}
			//}*/

			caller->OnResponseReceived(d);
		}

		/* should really check the image to make sure it is consistent,
		* that its sopClass and sopInstance correspond with those in
		* the request.
		*/
		if (rsp->DimseStatus == STATUS_Success) {
			/* which SOP class and SOP instance ? */
			if (! DU_findSOPClassAndInstanceInDataSet(d, sopClass, sopInstance)) {
				rsp->DimseStatus = STATUS_STORE_Error_CannotUnderstand;
			}
			else if (strcmp(sopClass, req->AffectedSOPClassUID) != 0) {
				rsp->DimseStatus = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
			}
			else if (strcmp(sopInstance, req->AffectedSOPInstanceUID) != 0) {
				rsp->DimseStatus = STATUS_STORE_Error_DataSetDoesNotMatchSOPClass;
			}
		}

	}

}

CONDITION DicomMoveAssociation::echoSCP(T_ASC_Association *assoc, T_DIMSE_Message *msg, T_ASC_PresentationContextID presID) {
	CONDITION cond;

	// the echo succeeded !!
	cond = DIMSE_sendEchoResponse(assoc, presID, &msg->msg.CEchoRQ, STATUS_Success, NULL);

	return cond;
}

void DicomMoveAssociation::OnResponseReceived(DcmDataset* response) {
	//delete response;
}
