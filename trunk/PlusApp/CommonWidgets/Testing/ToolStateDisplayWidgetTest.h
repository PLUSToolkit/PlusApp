/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#ifndef __ToolStateDisplayWidgetTest_h
#define __ToolStateDisplayWidgetTest_h

#include "PlusConfigure.h"
#include <QDialog>

class PlusDeviceSetSelectorWidget;
class PlusToolStateDisplayWidget;
class vtkPlusChannel;
class vtkPlusDataCollector;

//-----------------------------------------------------------------------------

/*! \class PlusToolStateDisplayWidgetTest 
 * \brief Qt application for testing PlusToolStateDisplayWidget
 * \ingroup PlusAppCommonWidgets
 */
class PlusToolStateDisplayWidgetTest : public QDialog
{
	Q_OBJECT

public:
	/*!
	* \brief Constructor
	* \param aParent parent
	* \param aFlags widget flag
	*/
	PlusToolStateDisplayWidgetTest(QWidget *parent = 0, Qt::WindowFlags flags = 0);

	/*!
	* \brief Destructor
	*/
	~PlusToolStateDisplayWidgetTest();

protected slots:
	/*!
	* \brief Connect to devices described in the argument configuration file in response by clicking on the Connect button
	* \param aConfigFile DeviceSet configuration file path and name
	*/
	void ConnectToDevicesByConfigFile(std::string aConfigFile);

  /*!
   * Refreshes the tool display widget
   */
  void RefreshToolDisplay();

protected:
	/*!
	 * \brief Read configuration file and start data collection
	 * \return Success flag
	 */
  PlusStatus StartDataCollection();

  /*! Select the channel */
  PlusStatus SelectChannel(vtkPlusChannel*& aChannel);

protected:
  /*! Device set selector widget */
	PlusDeviceSetSelectorWidget*	m_DeviceSetSelectorWidget;

	/*! Tool state display widget */
	PlusToolStateDisplayWidget*		m_ToolStateDisplayWidget;

	/*! Data source object */
	vtkPlusChannel*	        m_SelectedChannel;
  vtkPlusDataCollector*       m_DataCollector;

};

#endif // __ToolStateDisplayWidgetTest_h
