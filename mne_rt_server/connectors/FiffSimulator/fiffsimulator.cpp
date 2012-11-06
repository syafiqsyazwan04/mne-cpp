//=============================================================================================================
/**
* @file     fiffsimulator.cpp
* @author   Christoph Dinh <chdinh@nmr.mgh.harvard.edu>;
*           Matti H�m�l�inen <msh@nmr.mgh.harvard.edu>
* @version  1.0
* @date     July, 2012
*
* @section  LICENSE
*
* Copyright (C) 2012, Christoph Dinh and Matti Hamalainen. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that
* the following conditions are met:
*     * Redistributions of source code must retain the above copyright notice, this list of conditions and the
*       following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
*       the following disclaimer in the documentation and/or other materials provided with the distribution.
*     * Neither the name of the Massachusetts General Hospital nor the names of its contributors may be used
*       to endorse or promote products derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MASSACHUSETTS GENERAL HOSPITAL BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*
*
* @brief    Contains the implementation of the FiffSimulator Class.
*
*/


//*************************************************************************************************************
//=============================================================================================================
// INCLUDES
//=============================================================================================================

#include "fiffsimulator.h"
#include "fiffproducer.h"


//*************************************************************************************************************
//=============================================================================================================
// FIFF INCLUDES
//=============================================================================================================

#include "../../../MNE/fiff/fiff.h"
#include "../../../MNE/fiff/fiff_types.h"


//*************************************************************************************************************
//=============================================================================================================
// MNE INCLUDES
//=============================================================================================================

#include "../../../MNE/mne/mne.h"
#include "../../../MNE/mne/mne_epoch_data_list.h"


//*************************************************************************************************************
//=============================================================================================================
// QT INCLUDES
//=============================================================================================================

#include <QtCore/QtPlugin>

#include <QDebug>


//*************************************************************************************************************
//=============================================================================================================
// USED NAMESPACES
//=============================================================================================================

using namespace FiffSimulatorPlugin;
using namespace FIFFLIB;
using namespace MNELIB;


//*************************************************************************************************************
//=============================================================================================================
// DEFINE MEMBER METHODS
//=============================================================================================================

FiffSimulator::FiffSimulator()
: m_pFiffProducer(new FiffProducer())
, m_fSamplingRate(0)
, m_pRawInfo(NULL)
, m_sResourceDataPath("../../mne-cpp/bin/MNE-sample-data/MEG/sample/sample_audvis_raw.fif")
{

}


//*************************************************************************************************************

FiffSimulator::~FiffSimulator()
{
}


//*************************************************************************************************************

bool FiffSimulator::start()
{
    init();

    // Start threads
    m_pFiffProducer->start();

    QThread::start();

    return true;
}


//*************************************************************************************************************

bool FiffSimulator::stop()
{
    QThread::wait();
    return true;
}


//*************************************************************************************************************

ConnectorID FiffSimulator::getConnectorID() const
{
    return _FIFFSIMULATOR;
}


//*************************************************************************************************************

FiffInfo* FiffSimulator::getMeasInfo()
{

    if(!m_pRawInfo)
    {
        readRawInfo();
    }

    if(m_pRawInfo)
    {
        return m_pRawInfo->info;
    }
    else
    {
        return NULL;
    }
}


//*************************************************************************************************************

const char* FiffSimulator::getName() const
{
    return "Fiff File Simulator";
}


//*************************************************************************************************************

void FiffSimulator::init()
{

    QString t_sFileName = "../../mne-cpp/bin/MNE-sample-data/MEG/sample/sample_audvis_raw.fif";
    QFile* t_pFile = new QFile(t_sFileName);

    qint32 event = 1;
    QString t_sEventName = "../../mne-cpp/bin/MNE-sample-data/MEG/sample/sample_audvis_raw-eve.fif";





    float tmin = -1.5;
    float tmax = 1.5;

    bool keep_comp = false;
    fiff_int_t dest_comp = 0;
    bool pick_all  = true;

    qint32 k, p;

    //
    //   Setup for reading the raw data
    //
    FiffRawData* raw = NULL;
    if(!FiffStream::setup_read_raw(t_pFile, raw))
    {
        printf("Error during fiff setup raw read\n");
        return;
    }

    MatrixXi picks;
    if (pick_all)
    {
        //
        // Pick all
        //
        picks.resize(1,raw->info->nchan);

        for(k = 0; k < raw->info->nchan; ++k)
            picks(0,k) = k;
        //
    }
    else
    {
        QStringList include;
        include << "STI 014";
        bool want_meg   = true;
        bool want_eeg   = false;
        bool want_stim  = false;

//        picks = Fiff::pick_types(raw->info, want_meg, want_eeg, want_stim, include, raw->info->bads);
        picks = raw->info->pick_types(want_meg, want_eeg, want_stim, include, raw->info->bads);// prefer member function
    }

    QStringList ch_names;
    for(k = 0; k < picks.cols(); ++k)
        ch_names << raw->info->ch_names[picks(0,k)];

    //
    //   Set up projection
    //
    if (raw->info->projs.size() == 0)
        printf("No projector specified for these data\n");
    else
    {
        //
        //   Activate the projection items
        //
        for (k = 0; k < raw->info->projs.size(); ++k)
            raw->info->projs[k]->active = true;

        printf("%d projection items activated\n",raw->info->projs.size());
        //
        //   Create the projector
        //
//        fiff_int_t nproj = MNE::make_projector_info(raw->info, raw->proj); Using the member function instead
        qint32 nproj = raw->info->make_projector_info(raw->proj);

        if (nproj == 0)
        {
            printf("The projection vectors do not apply to these channels\n");
        }
        else
        {
            printf("Created an SSP operator (subspace dimension = %d)\n",nproj);
        }
    }

    //
    //   Set up the CTF compensator
    //
    qint32 current_comp = MNE::get_current_comp(raw->info);
    if (current_comp > 0)
        printf("Current compensation grade : %d\n",current_comp);

    if (keep_comp)
        dest_comp = current_comp;

    if (current_comp != dest_comp)
    {
        qDebug() << "This part needs to be debugged";
        if(MNE::make_compensator(raw->info, current_comp, dest_comp, raw->comp))
        {
            raw->info->chs = MNE::set_current_comp(raw->info->chs,dest_comp);
            printf("Appropriate compensator added to change to grade %d.\n",dest_comp);
        }
        else
        {
            printf("Could not make the compensator\n");
            return;
        }
    }




    //////////////////////////////////////////////////////







    //
    //  Read the events
    //
    MatrixXi events;
    QFile* t_pEventFile = NULL;
    if (t_sEventName.size() == 0)
    {
        p = t_sFileName.indexOf(".fif");
        if (p > 0)
        {
            t_sEventName = t_sFileName.replace(p, 4, "-eve.fif");
        }
        else
        {
            printf("Raw file name does not end properly\n");
            return;
        }
//        events = mne_read_events(t_sEventName);

        t_pEventFile = new QFile(t_sEventName);
        MNE::read_events(t_pEventFile, events);
        printf("Events read from %s\n",t_sEventName.toUtf8().constData());
    }
    else
    {
        //
        //   Binary file
        //
        p = t_sFileName.indexOf(".fif");
        if (p > 0)
        {
            t_pEventFile = new QFile(t_sEventName);
            if(!MNE::read_events(t_pEventFile, events))
            {
                printf("Error while read events.\n");
                return;
            }
            printf("Binary event file %s read\n",t_sEventName.toUtf8().constData());
        }
        else
        {
            //
            //   Text file
            //
            printf("Text file %s is not supported jet.\n",t_sEventName.toUtf8().constData());
//            try
//                events = load(eventname);
//            catch
//                error(me,mne_omit_first_line(lasterr));
//            end
//            if size(events,1) < 1
//                error(me,'No data in the event file');
//            end
//            //
//            //   Convert time to samples if sample number is negative
//            //
//            for p = 1:size(events,1)
//                if events(p,1) < 0
//                    events(p,1) = events(p,2)*raw.info.sfreq;
//                end
//            end
//            //
//            //    Select the columns of interest (convert to integers)
//            //
//            events = int32(events(:,[1 3 4]));
//            //
//            //    New format?
//            //
//            if events(1,2) == 0 && events(1,3) == 0
//                fprintf(1,'The text event file %s is in the new format\n',eventname);
//                if events(1,1) ~= raw.first_samp
//                    error(me,'This new format event file is not compatible with the raw data');
//                end
//            else
//                fprintf(1,'The text event file %s is in the old format\n',eventname);
//                //
//                //   Offset with first sample
//                //
//                events(:,1) = events(:,1) + raw.first_samp;
//            end
        }
    }
    //
    //    Select the desired events
    //
    qint32 count = 0;
    MatrixXi selected = MatrixXi::Zero(1, events.rows());
    for (p = 0; p < events.rows(); ++p)
    {
        if (events(p,1) == 0 && events(p,2) == event)
        {
            selected(0,count) = p;
            ++count;
        }
    }
    selected.conservativeResize(1, count);
    if (count > 0)
        printf("%d matching events found\n",count);
    else
    {
        printf("No desired events found.\n");
        return;
    }


    fiff_int_t event_samp, from, to;
    MatrixXd* timesDummy = NULL;

    MNEEpochDataList data;

    MNEEpochData* epochData = NULL;

    MatrixXd times;

    for (p = 0; p < count; ++p)
    {
        //
        //       Read a data segment
        //
        event_samp = events(selected(p),0);
        from = event_samp + tmin*raw->info->sfreq;
        to   = event_samp + floor(tmax*raw->info->sfreq + 0.5);

        epochData = new MNEEpochData();

        if(raw->read_raw_segment(epochData->epoch, timesDummy, from, to, picks))
        {
            if (p == 0)
            {
                times.resize(1, to-from+1);
                for (qint32 i = 0; i < times.cols(); ++i)
                    times(0, i) = ((float)(from-event_samp+i)) / raw->info->sfreq;
            }

            epochData->event = event;
            epochData->tmin = ((float)(from)-(float)(raw->first_samp))/raw->info->sfreq;
            epochData->tmax = ((float)(to)-(float)(raw->first_samp))/raw->info->sfreq;

            data.append(epochData);//List takes ownwership of the pointer - no delete need
        }
        else
        {
            printf("Can't read the event data segments");
            delete raw;
            return;
        }
    }

    if(data.size() > 0)
    {
        printf("Read %d epochs, %d samples each.\n",data.size(),data[0]->epoch->cols());

        //DEBUG
        std::cout << data[0]->epoch->block(0,0,10,10) << std::endl;
        qDebug() << data[0]->epoch->rows() << " x " << data[0]->epoch->cols();

        std::cout << times.block(0,0,1,10) << std::endl;
        qDebug() << times.rows() << " x " << times.cols();
    }

    delete t_pEventFile;
    delete raw;
    delete t_pFile;

}


//*************************************************************************************************************

bool FiffSimulator::readRawInfo()
{
    if(!m_pRawInfo)
    {
        QFile* t_pFile = new QFile(m_sResourceDataPath);

        if(!FiffStream::setup_read_raw(t_pFile, m_pRawInfo))
        {
            printf("Error: Not able to read raw info!\n");
            if(m_pRawInfo)
                delete m_pRawInfo;
            m_pRawInfo = NULL;

            delete t_pFile;
            return false;
        }

        delete t_pFile;
    }

    return true;
}


//*************************************************************************************************************

void FiffSimulator::run()
{
    while(true)
    {

    }
}