/*
 * ProgressBar.java
 *
 * Copyright (C) 2009-18 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */
package org.rstudio.core.client.widget;

import com.google.gwt.core.client.GWT;
import com.google.gwt.uibinder.client.UiBinder;
import com.google.gwt.uibinder.client.UiField;
import com.google.gwt.user.client.ui.Composite;
import com.google.gwt.user.client.ui.HTMLPanel;
import com.google.gwt.user.client.ui.HorizontalPanel;
import com.google.gwt.user.client.ui.Widget;

public class ProgressBar extends Composite
{
   private static ProgressBarUiBinder uiBinder = GWT.create(ProgressBarUiBinder.class);

   interface ProgressBarUiBinder extends UiBinder<Widget, ProgressBar>
   {
   }

   public ProgressBar()
   {
      initWidget(uiBinder.createAndBindUi(this));
   }
   
   public void setProgress(double percent)
   {
      bar_.setWidth(percent + "%");
   }
   
   public void setProgress(int units, int max)
   {
      double percent = ((double)units / (double)max) * 100.0;
      setProgress(percent);
   }
   
   @Override
   public void setHeight(String height)
   {
      progress_.setHeight(height);
      bar_.setHeight(height);
   }

   @UiField HTMLPanel bar_;
   @UiField HTMLPanel progress_;
   @UiField HorizontalPanel progressHost_;
}
